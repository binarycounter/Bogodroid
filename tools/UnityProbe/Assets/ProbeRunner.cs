// Bogodroid compatibility probe.
//
// Drop this file into any Unity project (an empty scene is enough), add it to a GameObject, make an Android
// IL2CPP / ARM64 / OpenGL ES 3 build, and run it through a DEBUG unityloader. It calls the platform-facing parts
// of Unity (the parts that end up in Java/JNI and libc) one after another, never crashes the run if one fails,
// then quits by itself. Feed the loader's log to tools/unity_probe_report.py to get the list of what the loader
// still has to implement.
//
// Pure-engine features (physics, animation, particles, shaders) do not depend on the loader; this file does not
// try to cover them. See README.md next to this file.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Scripting;
using UnityEngine.SceneManagement;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

[Preserve]
public partial class ProbeRunner : MonoBehaviour
{
    // Quit by itself when finished, so an unattended run ends on its own.
    public bool autoQuit = true;

    readonly List<string> lines = new List<string>();
    string current = "starting";
    int ok, failed;

    void Start()
    {
        DontDestroyOnLoad(gameObject);
        StartCoroutine(RunAll());
    }

    // ---- reporting -------------------------------------------------------------------------------------------

    void Record(string result, string name, string detail)
    {
        if (result == "OK") ok++; else failed++;
        string line = "[PROBE] " + result + " " + name + ": " + detail;
        lines.Add(line);
        Debug.Log(line); // ends up in the loader log as LOG[Unity]: [PROBE] ...
    }

    // Runs one probe, turning any exception into a FAIL line instead of aborting the run.
    void Try(string name, Func<string> body)
    {
        current = name;
        Debug.Log("[PROBE] BEGIN " + name); // the loader log shows what each JNI call belongs to
        try { Record("OK", name, body()); }
        catch (Exception e) { Record("FAIL", name, Describe(e)); }
    }

    // ---- the run ---------------------------------------------------------------------------------------------

    IEnumerator RunAll()
    {
        yield return null;

        Try("SystemInfo", ProbeSystemInfo);
        Try("Application", ProbeApplication);
        Try("Screen", ProbeScreen);
        Try("Files", ProbeFiles);
        Try("PlayerPrefs", ProbePlayerPrefs);
        Try("Culture/ICU", ProbeCulture);
        Try("Threads", ProbeThreads);
        Try("Sockets", ProbeSockets);
        Try("Network info", ProbeNetworkInfo);
        Try("Crypto", ProbeCrypto);
        Try("Graphics", ProbeGraphics);
        Try("Audio", ProbeAudio);
        Try("Microphone", ProbeMicrophone);
        Try("Camera devices", ProbeWebcam);
        Try("Input (legacy)", ProbeLegacyInput);
        Try("Input (Input System)", ProbeInputSystem);
        Try("Haptics/keyboard", ProbeHandheld);
        Try("Memory", ProbeMemory);
        Try("Scenes", ProbeScenes);
#if UNITY_ANDROID && !UNITY_EDITOR
        Try("Android Java: Build/System", ProbeJavaBasics);
        Try("Android Java: Activity", ProbeJavaActivity);
        Try("Android permissions", ProbePermissions);
        Try("Clipboard", ProbeClipboard);
#endif

        // Probes that need to span frames.
        current = "Frame timing";
        Debug.Log("[PROBE] BEGIN Frame timing");
        float start = Time.realtimeSinceStartup;
        int frames = 0;
        while (frames < 90) { frames++; yield return null; }
        float elapsed = Time.realtimeSinceStartup - start;
        Record("OK", "Frame timing", frames + " frames in " + elapsed.ToString("F2", CultureInfo.InvariantCulture) + "s = " +
            (frames / Mathf.Max(elapsed, 0.001f)).ToString("F1", CultureInfo.InvariantCulture) + " fps");

        Try("Graphics capabilities", ProbeGraphicsCaps);
        yield return ProbeGraphicsScenes();

        yield return ProbeStreamingAssets();
        yield return ProbeHttp();
        yield return ProbePackages();
        yield return ProbeLocation();

        // Last on purpose: these can leave the app (an intent) or ask for user input.
        Try("TouchScreenKeyboard", ProbeKeyboard);
        yield return null;
        Try("Application.OpenURL", delegate { Application.OpenURL("https://example.com"); return "called"; });
        yield return new WaitForSeconds(0.5f);

        Finish();
        if (autoQuit)
        {
            yield return new WaitForSeconds(1f);
            Debug.Log("[PROBE] calling Application.Quit()");
            Application.Quit();
        }
    }

    void Finish()
    {
        current = "done";
        string summary = "[PROBE] DONE ok=" + ok + " failed=" + failed;
        lines.Add(summary);
        Debug.Log(summary);
        try
        {
            File.WriteAllText(Path.Combine(Application.persistentDataPath, ReportFileName()), string.Join("\n", lines.ToArray()));
        }
        catch (Exception e) { Debug.Log("[PROBE] could not write report: " + e.Message); }
    }

    // On-screen progress, handy when watching the device (IMGUI needs nothing else).
    void OnGUI()
    {
        if (hideGui) return;
        GUI.matrix = Matrix4x4.Scale(new Vector3(1.5f, 1.5f, 1f));
        GUI.Label(new Rect(10, 10, Screen.width, 30), "Probe: " + current + "   ok=" + ok + " failed=" + failed);
        int shown = Mathf.Min(lines.Count, 14);
        for (int i = 0; i < shown; i++)
            GUI.Label(new Rect(10, 40 + i * 20, Screen.width, 22), lines[lines.Count - shown + i]);
    }

    // ---- probes: each returns a short description, or throws ----------------------------------------------------

    static string I(object o) { return Convert.ToString(o, CultureInfo.InvariantCulture); }

    string ProbeSystemInfo()
    {
        var sb = new StringBuilder();
        sb.Append("model=" + SystemInfo.deviceModel + " name=" + SystemInfo.deviceName + " type=" + SystemInfo.deviceType);
        sb.Append(" os=" + SystemInfo.operatingSystem + " cpu=" + SystemInfo.processorType + "x" + SystemInfo.processorCount);
        sb.Append(" ram=" + SystemInfo.systemMemorySize + "MB gpu=" + SystemInfo.graphicsDeviceName + " api=" + SystemInfo.graphicsDeviceType);
        sb.Append(" gfxmem=" + SystemInfo.graphicsMemorySize + " maxTex=" + SystemInfo.maxTextureSize);
        sb.Append(" compute=" + SystemInfo.supportsComputeShaders + " instancing=" + SystemInfo.supportsInstancing);
        sb.Append(" battery=" + SystemInfo.batteryLevel + "/" + SystemInfo.batteryStatus);
        sb.Append(" gyro=" + SystemInfo.supportsGyroscope + " accel=" + SystemInfo.supportsAccelerometer + " vib=" + SystemInfo.supportsVibration);
        sb.Append(" loc=" + SystemInfo.supportsLocationService + " audio=" + SystemInfo.supportsAudio + " uid=" + SystemInfo.deviceUniqueIdentifier);
        return sb.ToString();
    }

    string ProbeApplication()
    {
        return "platform=" + Application.platform + " version=" + Application.version + " id=" + Application.identifier +
            " unity=" + Application.unityVersion + " lang=" + Application.systemLanguage + " genuine=" + Application.genuine +
            " installer=" + Application.installerName + " install=" + Application.installMode + " net=" + Application.internetReachability +
            " focused=" + Application.isFocused + " persistent=" + Application.persistentDataPath + " data=" + Application.dataPath +
            " streaming=" + Application.streamingAssetsPath + " cache=" + Application.temporaryCachePath;
    }

    string ProbeScreen()
    {
        string before = Screen.width + "x" + Screen.height + " dpi=" + Screen.dpi + " orient=" + Screen.orientation +
            " safe=" + Screen.safeArea + " cutouts=" + Screen.cutouts.Length + " res=" + Screen.currentResolution;
        Screen.sleepTimeout = SleepTimeout.NeverSleep;
        Screen.orientation = ScreenOrientation.LandscapeLeft;
        Application.targetFrameRate = 60;
        QualitySettings.vSyncCount = 1;
        return before + " (set sleepTimeout/orientation/targetFrameRate/vSync)";
    }

    string ProbeFiles()
    {
        string dir = Path.Combine(Application.persistentDataPath, "probe");
        Directory.CreateDirectory(dir);
        string file = Path.Combine(dir, "probe.txt");
        File.WriteAllText(file, "hello");
        string back = File.ReadAllText(file);
        File.AppendAllText(file, " world");
        using (var fs = new FileStream(file, FileMode.Open, FileAccess.ReadWrite, FileShare.Read))
            fs.Seek(0, SeekOrigin.End);
        int count = Directory.GetFiles(dir).Length;
        DateTime written = File.GetLastWriteTime(file);
        File.Delete(file);
        string env = Environment.GetFolderPath(Environment.SpecialFolder.Personal) + " | " + Path.GetTempPath() + " | " +
            Environment.OSVersion + " | cpus=" + Environment.ProcessorCount + " | " + Environment.MachineName;
        return "wrote+read '" + back + "', files=" + count + ", mtime=" + written.Year + ", " + env;
    }

    string ProbePlayerPrefs()
    {
        // Run the app twice: the counter should be 2 the second time if SharedPreferences persists.
        int runs = PlayerPrefs.GetInt("probe_runs", 0) + 1;
        PlayerPrefs.SetInt("probe_runs", runs);
        PlayerPrefs.SetString("probe_string", "ünï©ode");
        PlayerPrefs.SetFloat("probe_float", 1.5f);
        PlayerPrefs.Save();
        return "probe_runs=" + runs + " string=" + PlayerPrefs.GetString("probe_string") + " float=" + PlayerPrefs.GetFloat("probe_float") +
            " hasKey=" + PlayerPrefs.HasKey("probe_runs");
    }

    string ProbeCulture()
    {
        var de = new CultureInfo("de-DE");
        var tr = new CultureInfo("tr-TR");
        string a = DateTime.Now.ToString("F", de);
        int cmp = string.Compare("straße", "STRASSE", de, CompareOptions.IgnoreCase);
        string up = "i".ToUpper(tr);
        string norm = "é".Normalize(NormalizationForm.FormD);
        string tz = TimeZoneInfo.Local.Id + " zones=" + TimeZoneInfo.GetSystemTimeZones().Count;
        return "current=" + CultureInfo.CurrentCulture.Name + " de='" + a + "' cmp=" + cmp + " trUpper=" + up + " nfd=" + norm.Length + " tz=" + tz;
    }

    string ProbeThreads()
    {
        int sum = 0;
        var t = new Thread(delegate () { Interlocked.Add(ref sum, 1); });
        t.Start(); t.Join();
        var task = Task.Run(delegate () { Interlocked.Add(ref sum, 10); return 5; });
        task.Wait();
        Parallel.For(0, 8, delegate (int i) { Interlocked.Add(ref sum, 100); });
        var sem = new SemaphoreSlim(0);
        ThreadPool.QueueUserWorkItem(delegate (object o) { sem.Release(); });
        bool signalled = sem.Wait(2000);
        return "sum=" + sum + " task=" + task.Result + " pool=" + signalled + " thread=" + Thread.CurrentThread.ManagedThreadId;
    }

    string ProbeSockets()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        int port = ((IPEndPoint)listener.LocalEndpoint).Port;
        using (var client = new TcpClient())
        {
            client.Connect(IPAddress.Loopback, port);
            using (var server = listener.AcceptTcpClient())
            {
                byte[] msg = Encoding.UTF8.GetBytes("ping");
                client.GetStream().Write(msg, 0, msg.Length);
                byte[] buf = new byte[16];
                int n = server.GetStream().Read(buf, 0, buf.Length);
                listener.Stop();
                return "loopback tcp port " + port + " echoed " + n + " bytes";
            }
        }
    }

    string ProbeNetworkInfo()
    {
        string host = Dns.GetHostName();
        var nics = System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces();
        return "host=" + host + " interfaces=" + nics.Length + " available=" + System.Net.NetworkInformation.NetworkInterface.GetIsNetworkAvailable();
    }

    string ProbeCrypto()
    {
        // In Unity 7 on Android these go through Java (the log shows AndroidCryptoNative_*): expect JNI traffic.
        byte[] data = Encoding.UTF8.GetBytes("bogodroid");
        string sha;
        using (var h = SHA256.Create()) sha = BitConverter.ToString(h.ComputeHash(data)).Replace("-", "").Substring(0, 12);
        string md5;
        using (var h = MD5.Create()) md5 = BitConverter.ToString(h.ComputeHash(data)).Replace("-", "").Substring(0, 8);
        byte[] rnd = new byte[8];
        using (var r = RandomNumberGenerator.Create()) r.GetBytes(rnd);
        string aes;
        using (var a = Aes.Create())
        {
            a.GenerateKey(); a.GenerateIV();
            using (var enc = a.CreateEncryptor()) aes = enc.TransformFinalBlock(data, 0, data.Length).Length + " bytes";
        }
        return "sha256=" + sha + " md5=" + md5 + " rng=" + rnd.Length + " aes=" + aes + " guid=" + Guid.NewGuid();
    }

    string ProbeGraphics()
    {
        var rt = RenderTexture.GetTemporary(256, 256, 24, RenderTextureFormat.ARGB32);
        var cam = new GameObject("probe-cam").AddComponent<Camera>();
        cam.targetTexture = rt;
        cam.clearFlags = CameraClearFlags.SolidColor;
        cam.backgroundColor = Color.magenta;
        cam.Render();
        var tex = new Texture2D(256, 256, TextureFormat.RGBA32, false);
        var prev = RenderTexture.active;
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, 256, 256), 0, 0);
        tex.Apply();
        RenderTexture.active = prev;
        Color px = tex.GetPixel(10, 10);
        byte[] png = tex.EncodeToPNG();
        Graphics.Blit(tex, rt);
        QualitySettings.antiAliasing = 4;
        QualitySettings.vSyncCount = 0;
        GL.Flush();
        string shaders = (Shader.Find("Sprites/Default") != null) + "/" + (Shader.Find("Hidden/BlitCopy") != null);
        cam.targetTexture = null;
        RenderTexture.ReleaseTemporary(rt);
        Destroy(cam.gameObject);
        return "rendered pixel=" + px + " png=" + png.Length + "B shaders=" + shaders + " msaa=" + QualitySettings.antiAliasing +
            " formats rgba16f=" + SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.ARGBHalf);
    }

    string ProbeAudio()
    {
        var cfg = AudioSettings.GetConfiguration();
        var clip = AudioClip.Create("probe", 48000, 1, 24000, false);
        float[] samples = new float[48000]; // 2 seconds
        for (int i = 0; i < samples.Length; i++) samples[i] = Mathf.Sin(i * 0.1f) * 0.4f;
        clip.SetData(samples, 0);
        var src = new GameObject("probe-audio").AddComponent<AudioSource>();
        src.clip = clip;
        src.volume = 0.6f; // a clear ~380 Hz beep for two seconds, so a listener can confirm sound works
        src.Play();
        AudioListener.volume = 1f;
        return "rate=" + AudioSettings.outputSampleRate + " buffer=" + cfg.dspBufferSize + " speakers=" + cfg.speakerMode + " playing=" + src.isPlaying;
    }

    string ProbeMicrophone()
    {
        string[] devices = Microphone.devices;
        string detail = "devices=" + devices.Length;
        if (devices.Length > 0)
        {
            var clip = Microphone.Start(devices[0], false, 1, 16000);
            detail += " started=" + (clip != null);
            Microphone.End(devices[0]);
        }
        return detail;
    }

    string ProbeWebcam()
    {
        return "webcams=" + WebCamTexture.devices.Length;
    }

    string ProbeLegacyInput()
    {
#if ENABLE_LEGACY_INPUT_MANAGER
        Input.multiTouchEnabled = true;
        Input.gyro.enabled = true;
        Input.compass.enabled = true;
        string axis;
        try { axis = I(Input.GetAxis("Horizontal")); } catch (Exception) { axis = "(no Horizontal axis)"; }
        return "touchSupported=" + Input.touchSupported + " touches=" + Input.touchCount + " mouse=" + Input.mousePresent +
            " joysticks=" + string.Join(",", Input.GetJoystickNames()) + " accel=" + Input.acceleration + " gyro=" + Input.gyro.attitude +
            " compass=" + Input.compass.enabled + " orient=" + Input.deviceOrientation + " backLeavesApp=" + Application.backButtonLeavesApp +
            " stylus=" + Input.stylusTouchSupported + " axis=" + axis;
#else
        return "legacy Input Manager is disabled in this project";
#endif
    }

    string ProbeInputSystem()
    {
#if ENABLE_INPUT_SYSTEM
        return "devices=" + InputSystem.devices.Count + " gamepads=" + Gamepad.all.Count + " touchscreen=" + (Touchscreen.current != null) +
            " keyboard=" + (Keyboard.current != null) + " mouse=" + (Mouse.current != null) + " accelerometer=" + (Accelerometer.current != null) +
            " gyro=" + (UnityEngine.InputSystem.Gyroscope.current != null);
#else
        return "Input System package is not enabled in this project";
#endif
    }

    string ProbeHandheld()
    {
        Handheld.Vibrate();
        return "vibrated, keyboardSupported=" + TouchScreenKeyboard.isSupported;
    }

    string ProbeKeyboard()
    {
        var kb = TouchScreenKeyboard.Open("probe");
        string detail = (kb == null) ? "Open returned null" : "opened, status=" + kb.status;
        if (kb != null) kb.active = false;
        return detail;
    }

    string ProbeMemory()
    {
        long managed = GC.GetTotalMemory(false);
        byte[] big = new byte[64 * 1024 * 1024]; // makes the allocator map real memory
        big[big.Length - 1] = 1;
        GC.Collect();
        Resources.UnloadUnusedAssets();
        return "managed=" + managed / 1024 + "KB allocated 64MB, native=" + UnityEngine.Profiling.Profiler.GetTotalAllocatedMemoryLong() / 1024 + "KB";
    }

    string ProbeScenes()
    {
        return "scenes in build=" + SceneManager.sceneCountInBuildSettings + " active=" + SceneManager.GetActiveScene().name;
    }

#if UNITY_ANDROID && !UNITY_EDITOR
    string ProbeJavaBasics()
    {
        using (var build = new AndroidJavaClass("android.os.Build"))
        using (var version = new AndroidJavaClass("android.os.Build$VERSION"))
        using (var system = new AndroidJavaClass("java.lang.System"))
        using (var locale = new AndroidJavaClass("java.util.Locale"))
        using (var uuid = new AndroidJavaClass("java.util.UUID"))
        using (var tz = new AndroidJavaClass("java.util.TimeZone"))
        {
            string model = build.GetStatic<string>("MODEL");
            int sdk = version.GetStatic<int>("SDK_INT");
            long ms = system.CallStatic<long>("currentTimeMillis");
            string loc = locale.CallStatic<AndroidJavaObject>("getDefault").Call<string>("toString");
            string id = uuid.CallStatic<AndroidJavaObject>("randomUUID").Call<string>("toString");
            string zone = tz.CallStatic<AndroidJavaObject>("getDefault").Call<string>("getID");
            return "model=" + model + " sdk=" + sdk + " ms=" + ms + " locale=" + loc + " uuid=" + id + " tz=" + zone;
        }
    }

    string ProbeJavaActivity()
    {
        using (var player = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
        using (var activity = player.GetStatic<AndroidJavaObject>("currentActivity"))
        {
            string pkg = activity.Call<string>("getPackageName");
            string files = activity.Call<AndroidJavaObject>("getFilesDir").Call<string>("getAbsolutePath");
            string cache = activity.Call<AndroidJavaObject>("getCacheDir").Call<string>("getAbsolutePath");
            var pm = activity.Call<AndroidJavaObject>("getPackageManager");
            string ver = pm.Call<AndroidJavaObject>("getPackageInfo", pkg, 0).Get<string>("versionName");
            bool hasTouch = pm.Call<bool>("hasSystemFeature", "android.hardware.touchscreen");
            var window = activity.Call<AndroidJavaObject>("getSystemService", "window");
            var conn = activity.Call<AndroidJavaObject>("getSystemService", "connectivity");
            var audio = activity.Call<AndroidJavaObject>("getSystemService", "audio");
            return "pkg=" + pkg + " files=" + files + " cache=" + cache + " version=" + ver + " touchFeature=" + hasTouch +
                " window=" + (window != null) + " connectivity=" + (conn != null) + " audio=" + (audio != null);
        }
    }

    string ProbePermissions()
    {
        bool mic = UnityEngine.Android.Permission.HasUserAuthorizedPermission("android.permission.RECORD_AUDIO");
        bool cam = UnityEngine.Android.Permission.HasUserAuthorizedPermission("android.permission.CAMERA");
        bool loc = UnityEngine.Android.Permission.HasUserAuthorizedPermission("android.permission.ACCESS_FINE_LOCATION");
        return "mic=" + mic + " camera=" + cam + " location=" + loc;
    }

    string ProbeClipboard()
    {
        GUIUtility.systemCopyBuffer = "bogodroid-probe";
        return "clipboard='" + GUIUtility.systemCopyBuffer + "'";
    }
#endif

    // ---- probes that span frames ---------------------------------------------------------------------------------

    IEnumerator ProbeStreamingAssets()
    {
        // Needs Assets/StreamingAssets/probe.txt (any text). On Android this path is inside the APK.
        current = "StreamingAssets";
        Debug.Log("[PROBE] BEGIN StreamingAssets");
        string path = Path.Combine(Application.streamingAssetsPath, "probe.txt");
        using (var req = UnityWebRequest.Get(path))
        {
            yield return req.SendWebRequest();
            if (req.result == UnityWebRequest.Result.Success)
                Record("OK", "StreamingAssets", path + " -> " + req.downloadHandler.text.Length + " chars");
            else
                Record("FAIL", "StreamingAssets", path + " -> " + req.error + " (add Assets/StreamingAssets/probe.txt)");
        }
    }

    IEnumerator ProbeHttp()
    {
        // No network is needed: a failure is still a useful probe of the networking stack.
        current = "HTTP";
        Debug.Log("[PROBE] BEGIN HTTP");
        using (var req = UnityWebRequest.Get("https://example.com/"))
        {
            req.timeout = 5;
            yield return req.SendWebRequest();
            Record(req.result == UnityWebRequest.Result.Success ? "OK" : "FAIL", "HTTP", "https://example.com -> " + req.result + " " + req.error);
        }
    }

    IEnumerator ProbeLocation()
    {
        current = "Location";
        Debug.Log("[PROBE] BEGIN Location");
#if ENABLE_LEGACY_INPUT_MANAGER
        if (!Input.location.isEnabledByUser)
        {
            Record("OK", "Location", "disabled by user / unavailable (status=" + Input.location.status + ")");
            yield break;
        }
        Input.location.Start();
        int wait = 3;
        while (Input.location.status == UnityEngine.LocationServiceStatus.Initializing && wait-- > 0)
            yield return new WaitForSeconds(1f);
        Record("OK", "Location", "status=" + Input.location.status);
        Input.location.Stop();
#else
        Record("OK", "Location", "legacy Input Manager disabled");
        yield break;
#endif
    }
}
