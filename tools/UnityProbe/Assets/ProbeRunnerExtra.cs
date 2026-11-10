// Second half of the probe: package-level features (Addressables, Localization, Notifications, IAP, Unity Gaming
// Services) and the graphics scene checks. See ProbeRunner.cs and Editor/ProbeSceneBuilder.cs.
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Rendering;

public partial class ProbeRunner
{
    // Filled in by ProbeSceneBuilder.
    public Camera urpCamera;
    public Camera rtCamera;
    public Camera builtinCamera;       // Built-in render pipeline + Post Processing v2 stage
    public GameObject builtinRoot;
    public GameObject builtinAoScalable;   // PPv2 Ambient Occlusion, fragment shaders only
    public GameObject builtinAoMultiScale; // PPv2 Ambient Occlusion, needs compute shaders
    public ReflectionProbe reflectionProbe;

    bool hideGui; // IMGUI text must not count as "the screen shows something" when checking the backbuffer
    string builtinFallback = "n/a";

    // Unity 7 only renders with the Built-in pipeline as a fallback that is off by default: with no render pipeline
    // asset and the fallback off, cameras draw nothing. The switch is internal, so flip it by reflection.
    void Awake()
    {
        if (GraphicsSettings.currentRenderPipeline != null) return;
        try
        {
            var flags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
            var set = typeof(GraphicsSettings).GetMethod("SetBuiltInRenderPipelineFallbackEnabled", flags);
            var get = typeof(GraphicsSettings).GetMethod("GetBuiltInRenderPipelineFallbackEnabled", flags);
            if (set == null) { builtinFallback = "no API (older Unity: always on)"; return; }
            set.Invoke(null, new object[] { true });
            builtinFallback = get != null ? "enabled=" + get.Invoke(null, null) : "set";
        }
        catch (Exception e) { builtinFallback = "error: " + e.GetType().Name + ": " + e.Message; }
        Debug.Log("[PROBE] built-in pipeline fallback: " + builtinFallback);
    }

    // Type: message, then each inner exception ("<-"), then where the innermost one was thrown: wrapper exceptions
    // like ServicesInitializationException say nothing useful on their own.
    static string Describe(Exception e)
    {
        var sb = new System.Text.StringBuilder();
        var seen = new System.Collections.Generic.HashSet<Exception>();
        Exception last = e;
        for (Exception cur = e; cur != null && seen.Add(cur); )
        {
            if (sb.Length > 0) sb.Append(" <- ");
            sb.Append(cur.GetType().Name).Append(": ").Append(cur.Message);
            last = cur;
            var agg = cur as AggregateException;
            cur = agg != null && agg.InnerExceptions.Count > 0 ? agg.InnerExceptions[0] : cur.InnerException;
        }
        if (!string.IsNullOrEmpty(last.StackTrace))
        {
            var frames = last.StackTrace.Split('\n');
            sb.Append(" | at");
            for (int i = 0; i < frames.Length && i < 3; i++) sb.Append(' ').Append(frames[i].Trim());
        }
        return sb.ToString().Replace('\n', ' ').Replace('\r', ' ');
    }

    // Both flavors run from the same port and share conf/, so the Built-in one gets its own report name.
    string ReportFileName()
    {
        return urpCamera == null && builtinCamera != null ? "probe_report-builtin.txt" : "probe_report.txt";
    }

    // ---- packages ------------------------------------------------------------------------------------------------

    // Runs an async probe with a timeout. (A coroutine can't yield inside try/catch, hence the Task juggling.)
    // expectUnavailable: online-only services (Unity Gaming Services, IAP) can't work on a handheld without Google Play
    // or a linked cloud project; a clean failure is the right result, so it's recorded as OK ("unavailable").
    IEnumerator RunAsync(string name, Func<Task<string>> start, float timeout = 15f, bool expectUnavailable = false)
    {
        current = name;
        Debug.Log("[PROBE] BEGIN " + name);
        Task<string> t = null;
        try { t = start(); }
        catch (Exception e) { Record("FAIL", name, Describe(e)); yield break; }
        float end = Time.realtimeSinceStartup + timeout;
        while (!t.IsCompleted && Time.realtimeSinceStartup < end) yield return null;
        if (!t.IsCompleted) Record("FAIL", name, "timed out after " + timeout + "s");
        else if (t.IsFaulted) Record(expectUnavailable ? "OK" : "FAIL", name, (expectUnavailable ? "unavailable, as expected offline: " : "") + Describe(t.Exception));
        else Record("OK", name, t.Result);
    }

    IEnumerator ProbePackages()
    {
        yield return RunAsync("Addressables init", async delegate
        {
            await UnityEngine.AddressableAssets.Addressables.InitializeAsync().Task;
            return "initialized";
        });

        yield return RunAsync("Localization init", async delegate
        {
            await UnityEngine.Localization.Settings.LocalizationSettings.InitializationOperation.Task;
            var l = UnityEngine.Localization.Settings.LocalizationSettings.SelectedLocale;
            return "locale=" + (l != null ? l.Identifier.Code : "none");
        });

        // Needs no cloud project to *try*: it exercises the network stack and Java (device id, connectivity) and
        // is expected to fail cleanly when offline or unlinked.
        yield return RunAsync("Unity Gaming Services init", async delegate
        {
            await Unity.Services.Core.UnityServices.InitializeAsync();
            return "state=" + Unity.Services.Core.UnityServices.State;
        }, 20f, expectUnavailable: true);

#if UNITY_ANDROID && !UNITY_EDITOR
        Try("Native plugin", ProbeNativePlugin);
        Try("Android notifications", ProbeNotifications);
        yield return ProbePlayBilling();
#endif
        // Purchasing: reflection, because the v5 API surface moves around between versions.
        yield return RunAsync("IAP store connect", async delegate
        {
            Type svc = FindType("UnityEngine.Purchasing.UnityIAPServices");
            if (svc == null) return "Unity IAP not present";
            object store = svc.GetMethod("StoreController", BindingFlags.Public | BindingFlags.Static, null, Type.EmptyTypes, null).Invoke(null, null);
            var connect = store.GetType().GetMethod("Connect", Type.EmptyTypes);
            if (connect == null) return "StoreController has no Connect()";
            await (Task)connect.Invoke(store, null);
            return "connected";
        }, 20f, expectUnavailable: true);
    }

#pragma warning disable UAC0005 // fine here: we only read types from the assemblies, no unload hazard
    static Type FindType(string name)
    {
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            Type t = asm.GetType(name, false);
            if (t != null) return t;
        }
        return null;
    }
#pragma warning restore UAC0005

#if UNITY_ANDROID && !UNITY_EDITOR
    [System.Runtime.InteropServices.DllImport("probeplugin")] static extern int probe_plugin_add(int a, int b);
    [System.Runtime.InteropServices.DllImport("probeplugin")] static extern int probe_plugin_state();

    // libprobeplugin.so (tools/UnityProbe/NativePlugin): loaded on demand through P/Invoke (dlopen: dependencies and
    // constructor, no JNI_OnLoad), then System.loadLibrary must call its JNI_OnLoad.
    string ProbeNativePlugin()
    {
        int sum = probe_plugin_add(2, 3);           // 2 + 3 + 42 from libprobedep.so
        int before = probe_plugin_state();
        using (var system = new AndroidJavaClass("java.lang.System"))
            system.CallStatic("loadLibrary", "probeplugin");
        int after = probe_plugin_state();
        bool constructed = (before & 1) != 0, jniBefore = (before & 2) != 0, jniAfter = (after & 2) != 0;
        string detail = "add=" + sum + " constructor=" + constructed + " JNI_OnLoad after dlopen=" + jniBefore + ", after loadLibrary=" + jniAfter;
        if (sum != 47 || !constructed || jniBefore || !jniAfter) throw new Exception("unexpected: " + detail);
        return detail;
    }

    class BillingStateListener : AndroidJavaProxy
    {
        public int code = -1;
        public string message;
        public BillingStateListener() : base("com.android.billingclient.api.BillingClientStateListener") {}
        void onBillingSetupFinished(AndroidJavaObject result)
        {
            code = result.Call<int>("getResponseCode");
            message = result.Call<string>("getDebugMessage");
        }
        void onBillingServiceDisconnected() { code = -2; }
    }

    class PurchasesListener : AndroidJavaProxy
    {
        public PurchasesListener() : base("com.android.billingclient.api.PurchasesUpdatedListener") {}
        void onPurchasesUpdated(AndroidJavaObject result, AndroidJavaObject purchases) {}
    }

    // Google Play Billing driven the way Unity IAP does it: without Google Play, startConnection() must call back with
    // BILLING_UNAVAILABLE (3) rather than hang or throw.
    IEnumerator ProbePlayBilling()
    {
        const string name = "Google Play Billing";
        current = name;
        Debug.Log("[PROBE] BEGIN " + name);
        BillingStateListener listener = null;
        AndroidJavaObject client = null;
        string err = null;
        try
        {
            using (var player = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
            using (var activity = player.GetStatic<AndroidJavaObject>("currentActivity"))
            using (var clientClass = new AndroidJavaClass("com.android.billingclient.api.BillingClient"))
            using (var paramsClass = new AndroidJavaClass("com.android.billingclient.api.PendingPurchasesParams"))
            using (var builder = clientClass.CallStatic<AndroidJavaObject>("newBuilder", activity))
            using (var pendingBuilder = paramsClass.CallStatic<AndroidJavaObject>("newBuilder"))
            {
                builder.Call<AndroidJavaObject>("setListener", new PurchasesListener()).Dispose();
                pendingBuilder.Call<AndroidJavaObject>("enableOneTimeProducts").Dispose();
                using (var pending = pendingBuilder.Call<AndroidJavaObject>("build"))
                    builder.Call<AndroidJavaObject>("enablePendingPurchases", pending).Dispose();
                client = builder.Call<AndroidJavaObject>("build");
                listener = new BillingStateListener();
                client.Call("startConnection", listener);
            }
        }
        catch (Exception e) { err = Describe(e); }
        if (err != null) { Record("FAIL", name, err); yield break; }
        float end = Time.realtimeSinceStartup + 5f;
        while (listener.code == -1 && Time.realtimeSinceStartup < end) yield return null;
        bool ready = client.Call<bool>("isReady");
        client.Dispose();
        if (listener.code == 3 && !ready) Record("OK", name, "BILLING_UNAVAILABLE as expected (" + listener.message + ")");
        else Record("FAIL", name, "setup finished with code " + listener.code + " ready=" + ready);
    }

    string ProbeNotifications()
    {
        var channel = new Unity.Notifications.Android.AndroidNotificationChannel
        {
            Id = "probe",
            Name = "Probe",
            Importance = Unity.Notifications.Android.Importance.Default,
            Description = "Bogodroid probe",
        };
        Unity.Notifications.Android.AndroidNotificationCenter.RegisterNotificationChannel(channel);
        var n = new Unity.Notifications.Android.AndroidNotification
        {
            Title = "Probe",
            Text = "hello",
            FireTime = DateTime.Now.AddMinutes(30),
        };
        int id = Unity.Notifications.Android.AndroidNotificationCenter.SendNotification(n, "probe");
        Unity.Notifications.Android.AndroidNotificationCenter.CancelAllScheduledNotifications();
        return "scheduled+cancelled id=" + id;
    }
#endif

    // ---- graphics ------------------------------------------------------------------------------------------------

    string ProbeGraphicsCaps()
    {
        var rp = GraphicsSettings.currentRenderPipeline;
        var sb = new System.Text.StringBuilder();
        sb.Append("api=").Append(SystemInfo.graphicsDeviceType).Append(' ').Append(SystemInfo.graphicsDeviceVersion);
        sb.Append(" pipeline=").Append(rp != null ? rp.name : "built-in (fallback " + builtinFallback + ")");
        sb.Append(" compute=").Append(SystemInfo.supportsComputeShaders);
        sb.Append(" instancing=").Append(SystemInfo.supportsInstancing);
        sb.Append(" maxTex=").Append(SystemInfo.maxTextureSize);
        sb.Append(" msaa=").Append(SystemInfo.GetRenderTextureSupportedMSAASampleCount(new RenderTextureDescriptor(256, 256, RenderTextureFormat.ARGB32, 24)));
        sb.Append(" hdrRT=").Append(SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.RGB111110Float)).Append('/').Append(SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.ARGBHalf));
        sb.Append(" shadows=").Append(SystemInfo.supportsShadows);
        sb.Append(" depthRT=").Append(SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.Depth));
        sb.Append(" astc=").Append(SystemInfo.SupportsTextureFormat(TextureFormat.ASTC_6x6));
        sb.Append(" etc2=").Append(SystemInfo.SupportsTextureFormat(TextureFormat.ETC2_RGBA8));
        sb.Append(" cameras=").Append(Camera.allCamerasCount);
        return sb.ToString();
    }

    // Which render pipeline the build uses is fixed at build time: tools/buildprobe.sh builds a URP flavor (the scene has
    // urpCamera) and, with PIPELINE=builtin, a Built-in + Post Processing v2 flavor (the scene has only builtinCamera).
    IEnumerator ProbeGraphicsScenes()
    {
        if (urpCamera != null)
        {
            // Lights, shadows, transparency, particles, skybox, URP post-processing, SSAO, uGUI.
            current = "Graphics scene (URP)";
            Debug.Log("[PROBE] BEGIN Graphics scene (URP)");
            if (reflectionProbe != null) { try { reflectionProbe.RenderProbe(); } catch (Exception e) { Debug.Log("[PROBE] reflection probe: " + e.Message); } }
            yield return CheckScreen("Graphics scene (URP)", "probe_urp.png", 12);

            // Offscreen camera -> RenderTexture (shown on a quad in the scene) must also have produced something.
            if (rtCamera != null && rtCamera.targetTexture != null)
            {
                Try("Graphics RenderTexture camera", delegate
                {
                    rtCamera.Render();
                    return AnalyzeRT(rtCamera.targetTexture);
                });
            }
        }
        else if (builtinCamera != null)
        {
            // What a pre-URP game would use: Built-in pipeline + Post Processing v2, then its Ambient Occlusion modes.
            current = "Graphics scene (built-in + PPv2)";
            Debug.Log("[PROBE] BEGIN Graphics scene (built-in + PPv2)");
            yield return CheckScreen("Graphics scene (built-in + PPv2)", "probe_builtin.png", 12);
            if (builtinAoScalable != null)
            {
                builtinAoScalable.SetActive(true);
                yield return CheckScreen("Graphics scene (PPv2 + SAO)", "probe_builtin_sao.png", 8);
                builtinAoScalable.SetActive(false);
            }
            if (builtinAoMultiScale != null)
            {
                if (!SystemInfo.supportsComputeShaders)
                    Record("OK", "Graphics scene (PPv2 + MSVO)", "skipped: device has no compute shader support");
                else
                {
                    builtinAoMultiScale.SetActive(true);
                    yield return CheckScreen("Graphics scene (PPv2 + MSVO)", "probe_builtin_msvo.png", 8);
                    builtinAoMultiScale.SetActive(false);
                }
            }
        }
        else
            Record("FAIL", "Graphics scene", "scene has no camera: rebuild it with the Probe menu");
    }

    // Waits a few frames for the scene to settle, reads the back buffer, and fails when it is blank. Also saves a PNG
    // next to the probe report so the picture can be looked at afterwards.
    IEnumerator CheckScreen(string name, string file, int settleFrames)
    {
        hideGui = true;
        for (int i = 0; i < settleFrames; i++) yield return null;
        yield return new WaitForEndOfFrame();
        string result = null; bool fail = false;
        try
        {
            // What Unity presents, with its own colour-space handling (Linear projects otherwise come out washed out).
            Texture2D shot = ScreenCapture.CaptureScreenshotAsTexture();
            int w = shot.width, h = shot.height;
            result = Analyze(shot.GetPixels32(), w, h, out fail) + " " + w + "x" + h;
            File.WriteAllBytes(Path.Combine(Application.persistentDataPath, file), shot.EncodeToPNG());
            Destroy(shot);

            // The raw back buffer read for comparison: if this one looks different, the readback (or the EGL
            // surface's colour space) is off rather than the rendering.
            var raw = new Texture2D(Screen.width, Screen.height, TextureFormat.RGBA32, false);
            raw.ReadPixels(new Rect(0, 0, Screen.width, Screen.height), 0, 0);
            raw.Apply();
            File.WriteAllBytes(Path.Combine(Application.persistentDataPath, Path.GetFileNameWithoutExtension(file) + "_rp.png"), raw.EncodeToPNG());
            Destroy(raw);
        }
        catch (Exception e) { result = e.GetType().Name + ": " + e.Message; fail = true; }
        hideGui = false;
        Record(fail ? "FAIL" : "OK", name, result + " -> " + file + " (+ _rp.png raw readback) colorSpace=" + QualitySettings.activeColorSpace);
    }

    string AnalyzeRT(RenderTexture rt)
    {
        var prev = RenderTexture.active;
        var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        RenderTexture.active = prev;
        bool fail;
        string r = Analyze(tex.GetPixels32(), rt.width, rt.height, out fail);
        Destroy(tex);
        if (fail) throw new Exception(r);
        return r;
    }

    static string Analyze(Color32[] px, int w, int h, out bool blank)
    {
        var seen = new System.Collections.Generic.HashSet<int>();
        double sum = 0, sum2 = 0; int n = 0;
        for (int y = 0; y < h; y += Mathf.Max(1, h / 48))
            for (int x = 0; x < w; x += Mathf.Max(1, w / 64))
            {
                Color32 c = px[y * w + x];
                seen.Add((c.r >> 4) << 8 | (c.g >> 4) << 4 | (c.b >> 4));
                double l = (c.r + c.g + c.b) / 3.0;
                sum += l; sum2 += l * l; n++;
            }
        double mean = sum / n, sd = Math.Sqrt(Math.Max(0, sum2 / n - mean * mean));
        blank = seen.Count <= 3 || sd < 2.0;
        return (blank ? "BLANK " : "") + "colors=" + seen.Count + " meanLum=" + mean.ToString("F0", CultureInfo.InvariantCulture) +
            " sd=" + sd.ToString("F0", CultureInfo.InvariantCulture);
    }
}
