// Headless Android build for the Bogodroid probe, used by tools/buildprobe.sh:
//   Unity -batchmode -nographics -quit -projectPath tools/UnityProbe -executeMethod ProbeBuild.BuildAndroid -probeOut <apk>
// It forces every setting the probe depends on (so the result doesn't depend on what was last clicked in the
// editor), most importantly Managed Stripping = Disabled so IL2CPP keeps all the APIs the probe calls.
using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine.Rendering;

public static class ProbeBuild
{
    const string Scene = "Assets/Scenes/Probe.unity";

    // The native test plugin built by tools/buildprobe.sh: Android ARM64 only.
    static void ConfigureNativePlugins()
    {
        AssetDatabase.Refresh();
        foreach (string path in new[] { "Assets/Plugins/Android/libprobedep.so", "Assets/Plugins/Android/libprobeplugin.so" })
        {
            var importer = AssetImporter.GetAtPath(path) as PluginImporter;
            if (importer == null) { Console.Error.WriteLine("Native test plugin missing: " + path); continue; }
            importer.SetCompatibleWithAnyPlatform(false);
            importer.SetCompatibleWithEditor(false);
            importer.SetCompatibleWithPlatform(BuildTarget.Android, true);
            importer.SetPlatformData(BuildTarget.Android, "CPU", "ARM64");
            importer.SaveAndReimport();
        }
    }

    public static void BuildAndroid()
    {
        string output = "Build/probe.apk";
        bool builtin = false;
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length - 1; i++)
        {
            if (args[i] == "-probeOut") output = args[i + 1];
            if (args[i] == "-probePipeline") builtin = args[i + 1] == "builtin";
        }
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output)));

        ProbeSceneBuilder.Build(builtin);
        ConfigureNativePlugins();

        var target = NamedBuildTarget.Android;
        PlayerSettings.SetScriptingBackend(target, ScriptingImplementation.IL2CPP);
        PlayerSettings.SetManagedStrippingLevel(target, ManagedStrippingLevel.Disabled);
        PlayerSettings.Android.targetArchitectures = AndroidArchitecture.ARM64;
        PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android, false);
        PlayerSettings.SetGraphicsAPIs(BuildTarget.Android, new[] { GraphicsDeviceType.OpenGLES3 });
        PlayerSettings.Android.applicationEntry = AndroidApplicationEntry.Activity;
        PlayerSettings.SetApplicationIdentifier(target, "com.GeoffWoollams.UnityProbe");
        EditorUserBuildSettings.buildAppBundle = false;
        EditorUserBuildSettings.androidBuildSystem = AndroidBuildSystem.Gradle;

        var options = new BuildPlayerOptions
        {
            scenes = new[] { Scene },
            locationPathName = output,
            target = BuildTarget.Android,
            targetGroup = BuildTargetGroup.Android,
            options = BuildOptions.Development,
        };

        BuildSummary summary = BuildPipeline.BuildPlayer(options).summary;
        if (summary.result != BuildResult.Succeeded)
        {
            Console.Error.WriteLine("Probe build failed: " + summary.result + " (" + summary.totalErrors + " errors)");
            EditorApplication.Exit(1);
        }
    }
}
