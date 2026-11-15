// Soak mode for the probe: after the normal probes it keeps the app busy for a long time, so that rare crashes in the
// loader (threads, JNI, file access, frame callbacks) get a chance to show up on a desktop machine instead of in a
// user's hands. It does the kinds of things a real game does continuously: renders frames, allocates and collects
// garbage, reads and writes files, uses worker threads and PlayerPrefs.
//
// Switch it on with PROBE_SOAK_SECONDS=<n> in the environment of the loader, or a file soak.txt containing <n> in
// the game's persistent data folder. 0 / absent: the probe behaves exactly as before.
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public partial class ProbeRunner
{
    static int SoakSeconds()
    {
        int n = 0;
        try
        {
            string env = Environment.GetEnvironmentVariable("PROBE_SOAK_SECONDS");
            if (!string.IsNullOrEmpty(env) && int.TryParse(env.Trim(), out n) && n > 0) return n;
        }
        catch (Exception) { }
        try
        {
            string file = Path.Combine(Application.persistentDataPath, "soak.txt");
            if (File.Exists(file) && int.TryParse(File.ReadAllText(file).Trim(), out n) && n > 0) return n;
        }
        catch (Exception) { }
        return 0;
    }

    IEnumerator SoakIfRequested()
    {
        int seconds = SoakSeconds();
        if (seconds <= 0) yield break;

        current = "soak";
        Debug.Log("[SOAK] BEGIN " + seconds + " s");
        float begin = Time.realtimeSinceStartup;
        float lastBeat = begin, lastSecond = begin, lastGc = begin;
        long frames = 0, framesAtBeat = 0, hitches = 0, tasks = 0;
        float worst = 0;
        int counter = 0;
        string dir = Path.Combine(Application.persistentDataPath, "soak");
        try { Directory.CreateDirectory(dir); } catch (Exception e) { Debug.Log("[SOAK] no soak folder: " + e.Message); }
        byte[] keep = null;

        while (Time.realtimeSinceStartup - begin < seconds)
        {
            frames++;
            float dt = Time.unscaledDeltaTime;
            if (dt > worst) worst = dt;
            if (dt > 0.25f) hitches++;

            // Garbage every frame, as any game produces.
            byte[] garbage = new byte[256 + (int)(frames % 2048)];
            garbage[0] = (byte)frames;
            string s = "frame " + frames.ToString(CultureInfo.InvariantCulture) + " " + garbage.Length;
            if (s.Length == 0) keep = garbage; // keeps the optimiser from removing the work

            float now = Time.realtimeSinceStartup;

            // Once a second: file I/O and preferences, and a burst of worker-thread jobs.
            if (now - lastSecond >= 1f)
            {
                lastSecond = now;
                counter++;
                try
                {
                    string file = Path.Combine(dir, "soak" + (counter % 8) + ".bin");
                    byte[] data = new byte[64 * 1024];
                    for (int i = 0; i < data.Length; i += 512) data[i] = (byte)counter;
                    File.WriteAllBytes(file, data);
                    byte[] back = File.ReadAllBytes(file);
                    if (back.Length != data.Length) Debug.Log("[SOAK] FAIL file round trip");
                    PlayerPrefs.SetInt("soak_counter", counter);
                    PlayerPrefs.SetString("soak_text", s);
                    if (counter % 10 == 0) PlayerPrefs.Save();
                }
                catch (Exception e) { Debug.Log("[SOAK] FAIL io: " + e.Message); }

                for (int t = 0; t < 4; t++)
                {
                    int id = t;
                    Task.Run(() =>
                    {
                        try
                        {
                            double acc = 0;
                            for (int i = 0; i < 20000; i++) acc += Math.Sqrt(i + id);
                            Interlocked.Increment(ref tasksDone);
                            return acc;
                        }
                        catch (Exception) { Interlocked.Increment(ref tasksFailedShared); return 0.0; }
                    });
                    tasks++;
                }
            }

            // Every few seconds a full collection, and a short-lived thread, the way loading screens behave.
            if (now - lastGc >= 5f)
            {
                lastGc = now;
                GC.Collect();
                var th = new Thread(() => Thread.Sleep(5));
                th.Start();
                th.Join();
            }

            // Heartbeat: if the log stops here the loader (or the machine) died; the numbers show slow drifts.
            if (now - lastBeat >= 10f)
            {
                float span = now - lastBeat;
                Debug.Log("[SOAK] t=" + ((int)(now - begin)).ToString(CultureInfo.InvariantCulture) + "s frames=" + frames +
                    " fps=" + ((frames - framesAtBeat) / Mathf.Max(span, 0.001f)).ToString("F1", CultureInfo.InvariantCulture) +
                    " worstFrame=" + (worst * 1000f).ToString("F0", CultureInfo.InvariantCulture) + "ms hitches=" + hitches +
                    " tasks=" + Interlocked.Read(ref tasksDone) + "/" + tasks + " failed=" + Interlocked.Read(ref tasksFailedShared) +
                    " managedMB=" + (GC.GetTotalMemory(false) / (1024 * 1024)).ToString(CultureInfo.InvariantCulture));
                lastBeat = now;
                framesAtBeat = frames;
                worst = 0;
            }
            yield return null;
        }
        GC.KeepAlive(keep);
        Debug.Log("[SOAK] DONE frames=" + frames + " hitches=" + hitches + " tasksFailed=" + Interlocked.Read(ref tasksFailedShared));
    }

    long tasksDone, tasksFailedShared;
}
