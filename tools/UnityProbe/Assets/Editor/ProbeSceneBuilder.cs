// Generates Assets/Scenes/Probe.unity so the graphics probe is reproducible: a URP scene with a camera using
// post-processing, HDR, MSAA, soft shadows, several lights, opaque + transparent materials, a compressed
// texture, particles, skybox, a reflection probe, a render-texture camera and uGUI. A disabled second branch
// ("BuiltinRoot") holds a Built-in pipeline camera with Post Processing v2 that ProbeRunner switches to at runtime.
// Menu: Probe > Rebuild Probe Scene. tools/buildprobe.sh runs it before every build.
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.PostProcessing;
using UnityEngine.Rendering.Universal;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using UnityEditor.Localization;
using UnityEngine.Localization;
using UnityEngine.Localization.Settings;
using PPBloom = UnityEngine.Rendering.PostProcessing.Bloom;
using PPVignette = UnityEngine.Rendering.PostProcessing.Vignette;
using PPDof = UnityEngine.Rendering.PostProcessing.DepthOfField;

public static class ProbeSceneBuilder
{
    const string Dir = "Assets/Scenes/ProbeAssets";
    const string ScenePath = "Assets/Scenes/Probe.unity";

    [MenuItem("Probe/Rebuild Probe Scene (URP)")]
    public static void Build() { Build(false); }

    // Built-in flavor: clears every render pipeline asset (edits ProjectSettings; tools/buildprobe.sh restores them) so
    // the build uses the Built-in pipeline, and keeps only the Built-in + Post Processing v2 part of the scene.
    [MenuItem("Probe/Rebuild Probe Scene (Built-in, edits settings)")]
    public static void BuildBuiltin() { Build(true); }

    public static void Build(bool builtin)
    {
        Directory.CreateDirectory(Dir);
        ConfigureUrpAssets();
        if (builtin) ClearPipelines();
        EnsureLocalization();

        Scene scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

        // Assets
        Texture2D checker = MakeCheckerTexture();
        Material ground = Lit("ProbeGround", new Color(0.8f, 0.8f, 0.8f), 0f, 0.2f, checker);
        Material metal = Lit("ProbeMetal", new Color(0.9f, 0.7f, 0.3f), 1f, 0.9f, null);
        Material red = Lit("ProbeRed", new Color(0.8f, 0.1f, 0.1f), 0f, 0.5f, null);
        Material glass = Lit("ProbeGlass", new Color(0.3f, 0.6f, 1f, 0.4f), 0f, 0.9f, null);
        MakeTransparent(glass);
        Material sky = Save(new Material(Shader.Find("Skybox/Procedural")), "ProbeSky");
        RenderSettings.skybox = sky;
        RenderSettings.ambientMode = AmbientMode.Skybox;
        var rt = new RenderTexture(256, 256, 24, RenderTextureFormat.ARGB32) { name = "ProbeRT" };
        AssetDatabase.CreateAsset(rt, Dir + "/ProbeRT.renderTexture");
        Material rtMat = Lit("ProbeRTQuad", Color.white, 0f, 0f, rt);

        // Lights
        var sun = NewLight("Sun", LightType.Directional, new Vector3(50, -30, 0), Vector3.zero, 1.2f, Color.white);
        sun.shadows = LightShadows.Soft;
        var point = NewLight("Point", LightType.Point, Vector3.zero, new Vector3(-3, 1.5f, 0), 3f, Color.red);
        point.range = 8; point.shadows = LightShadows.Soft;
        var spot = NewLight("Spot", LightType.Spot, new Vector3(60, 0, 0), new Vector3(3, 5, 0), 6f, Color.cyan);
        spot.range = 12; spot.spotAngle = 50; spot.shadows = LightShadows.Soft;

        // Geometry
        Prim("Ground", PrimitiveType.Plane, new Vector3(0, 0, 0), new Vector3(2, 1, 2), ground);
        Prim("Sphere", PrimitiveType.Sphere, new Vector3(-1.5f, 0.5f, 0), Vector3.one, metal);
        Prim("Cube", PrimitiveType.Cube, new Vector3(0.5f, 0.5f, 0.5f), Vector3.one, red);
        Prim("Glass", PrimitiveType.Capsule, new Vector3(2.2f, 1f, -0.5f), Vector3.one, glass);
        var quad = Prim("RTQuad", PrimitiveType.Quad, new Vector3(0, 2.5f, 4), new Vector3(3, 3, 1), rtMat);
        quad.transform.rotation = Quaternion.Euler(0, 180, 0);

        // Particles
        var ps = new GameObject("Particles").AddComponent<ParticleSystem>();
        ps.transform.position = new Vector3(0, 0.2f, -1);
        var psr = ps.GetComponent<ParticleSystemRenderer>();
        var psMat = Save(new Material(Shader.Find("Universal Render Pipeline/Particles/Unlit")), "ProbeParticles");
        psMat.SetColor("_BaseColor", new Color(1, 0.6f, 0.1f, 1));
        psr.sharedMaterial = psMat;

        // Reflection probe
        var rpObj = new GameObject("ReflectionProbe");
        rpObj.transform.position = new Vector3(0, 1.5f, 0);
        var rp = rpObj.AddComponent<ReflectionProbe>();
        rp.mode = ReflectionProbeMode.Realtime;
        rp.refreshMode = ReflectionProbeRefreshMode.ViaScripting;
        rp.resolution = 128;
        rp.size = new Vector3(20, 10, 20);

        // Cameras
        var camObj = new GameObject("Main Camera") { tag = "MainCamera" };
        camObj.transform.SetPositionAndRotation(new Vector3(0, 2.5f, -6), Quaternion.Euler(12, 0, 0));
        var cam = camObj.AddComponent<Camera>();
        cam.allowHDR = true; cam.allowMSAA = true;
        camObj.AddComponent<AudioListener>();
        var data = camObj.AddComponent<UniversalAdditionalCameraData>();
        data.renderPostProcessing = true;
        data.renderShadows = true;
        data.antialiasing = AntialiasingMode.FastApproximateAntialiasing;
        data.requiresDepthOption = CameraOverrideOption.On;
        data.requiresColorOption = CameraOverrideOption.On;

        var rtObj = new GameObject("RT Camera");
        rtObj.transform.SetPositionAndRotation(new Vector3(0, 6, -3), Quaternion.Euler(60, 0, 0));
        var rtCam = rtObj.AddComponent<Camera>();
        rtCam.targetTexture = rt;
        rtObj.AddComponent<UniversalAdditionalCameraData>().renderPostProcessing = false;

        // URP post-processing volume
        var volObj = new GameObject("URP Volume");
        var vol = volObj.AddComponent<Volume>();
        vol.isGlobal = true;
        vol.sharedProfile = MakeUrpProfile();

        // uGUI overlay (font rendering, canvas batching)
        var canvasObj = new GameObject("Canvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        canvasObj.GetComponent<Canvas>().renderMode = RenderMode.ScreenSpaceOverlay;
        var img = new GameObject("Panel", typeof(Image)); img.transform.SetParent(canvasObj.transform, false);
        img.GetComponent<Image>().color = new Color(0, 0, 0, 0.4f);
        var rtf = img.GetComponent<RectTransform>(); rtf.anchorMin = new Vector2(0.7f, 0.8f); rtf.anchorMax = new Vector2(1, 1); rtf.offsetMin = rtf.offsetMax = Vector2.zero;
        var txt = new GameObject("Label", typeof(Text)); txt.transform.SetParent(img.transform, false);
        var t = txt.GetComponent<Text>();
        t.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        t.text = "uGUI text 0123 ÄÖÜ"; t.color = Color.white; t.alignment = TextAnchor.MiddleCenter; t.fontSize = 28;
        var trt = txt.GetComponent<RectTransform>(); trt.anchorMin = Vector2.zero; trt.anchorMax = Vector2.one; trt.offsetMin = trt.offsetMax = Vector2.zero;

        // Built-in pipeline + Post Processing v2 branch (inactive until ProbeRunner switches pipelines)
        var root = new GameObject("BuiltinRoot");
        var std = Save(new Material(Shader.Find("Standard")) { color = new Color(0.2f, 0.7f, 0.3f) }, "ProbeStandard");
        std.SetFloat("_Metallic", 0.6f); std.SetFloat("_Glossiness", 0.7f);
        var stdGround = Save(new Material(Shader.Find("Standard")) { mainTexture = checker }, "ProbeStandardGround");
        Prim("BGround", PrimitiveType.Plane, Vector3.zero, new Vector3(2, 1, 2), stdGround).transform.SetParent(root.transform, true);
        Prim("BSphere", PrimitiveType.Sphere, new Vector3(-1, 0.5f, 0), Vector3.one, std).transform.SetParent(root.transform, true);
        Prim("BCube", PrimitiveType.Cube, new Vector3(1, 0.5f, 0), Vector3.one, std).transform.SetParent(root.transform, true);
        var bl = NewLight("BSun", LightType.Directional, new Vector3(50, -30, 0), Vector3.zero, 1f, Color.white);
        bl.shadows = LightShadows.Soft; bl.transform.SetParent(root.transform, true);
        var bcObj = new GameObject("Builtin Camera");
        bcObj.transform.SetParent(root.transform, false);
        bcObj.transform.SetPositionAndRotation(new Vector3(0, 2.5f, -6), Quaternion.Euler(12, 0, 0));
        var bcam = bcObj.AddComponent<Camera>();
        bcam.allowHDR = true; bcam.enabled = builtin;
        if (builtin) { bcObj.tag = "MainCamera"; bcObj.AddComponent<AudioListener>(); }
        var layer = bcObj.AddComponent<PostProcessLayer>();
        layer.Init(FindPpResources());
        layer.volumeTrigger = bcObj.transform;
        layer.volumeLayer = ~0;
        layer.antialiasingMode = PostProcessLayer.Antialiasing.FastApproximateAntialiasing;
        var ppv = new GameObject("PP Volume").AddComponent<PostProcessVolume>();
        ppv.transform.SetParent(root.transform, false);
        ppv.isGlobal = true;
        ppv.sharedProfile = MakePpProfile();
        var aoSao = MakeAoVolume(root.transform, "PP AO (SAO)", AmbientOcclusionMode.ScalableAmbientObscurance);
        var aoMs = MakeAoVolume(root.transform, "PP AO (MSVO)", AmbientOcclusionMode.MultiScaleVolumetricObscurance);
        root.SetActive(builtin);

        // The probe itself
        var probe = new GameObject("Probe").AddComponent<ProbeRunner>();
        probe.urpCamera = cam; probe.rtCamera = rtCam; probe.builtinCamera = bcam; probe.builtinRoot = root; probe.builtinAoScalable = aoSao; probe.builtinAoMultiScale = aoMs; probe.reflectionProbe = rp;
        if (builtin)
        {
            // Drop everything that needs URP, leaving the Built-in branch, the uGUI canvas and the probe.
            probe.urpCamera = null; probe.rtCamera = null; probe.reflectionProbe = null;
            foreach (var go in scene.GetRootGameObjects())
                if (go.name != "Probe" && go.name != "BuiltinRoot" && go.name != "Canvas")
                    Object.DestroyImmediate(go);
        }

        EditorSceneManager.SaveScene(scene, ScenePath);
        AssetDatabase.SaveAssets();
        Debug.Log("Probe scene rebuilt: " + ScenePath);
    }

    // The Localization probe needs settings with at least one locale (locales are loaded through Addressables).
    static void EnsureLocalization()
    {
        var settings = LocalizationEditorSettings.ActiveLocalizationSettings;
        if (settings == null)
        {
            settings = ScriptableObject.CreateInstance<LocalizationSettings>();
            AssetDatabase.CreateAsset(settings, Dir + "/ProbeLocalizationSettings.asset");
            LocalizationEditorSettings.ActiveLocalizationSettings = settings;
        }
        foreach (string code in new[] { "en", "de" })
        {
            string path = Dir + "/Locale_" + code + ".asset";
            if (AssetDatabase.LoadAssetAtPath<Locale>(path) != null) continue;
            var locale = Locale.CreateLocale(code);
            AssetDatabase.CreateAsset(locale, path);
            LocalizationEditorSettings.AddLocale(locale);
        }
        AssetDatabase.SaveAssets();
    }

    // Unity refuses a build whose Graphics default and quality levels disagree about the pipeline, so "built-in" means
    // clearing all of them.
    static void ClearPipelines()
    {
        var so = new SerializedObject(AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/QualitySettings.asset")[0]);
        var levels = so.FindProperty("m_QualitySettings");
        for (int i = 0; i < levels.arraySize; i++)
            levels.GetArrayElementAtIndex(i).FindPropertyRelative("customRenderPipeline").objectReferenceValue = null;
        so.ApplyModifiedPropertiesWithoutUndo();
        GraphicsSettings.defaultRenderPipeline = null;
        AssetDatabase.SaveAssets();
    }

    // Make the URP asset(s) actually exercise the features the probe wants to see fail.
    static void ConfigureUrpAssets()
    {
        for (int i = 0; i < QualitySettings.names.Length; i++)
            Configure(QualitySettings.GetRenderPipelineAssetAt(i) as UniversalRenderPipelineAsset);
        Configure(GraphicsSettings.defaultRenderPipeline as UniversalRenderPipelineAsset);
        AssetDatabase.SaveAssets();
    }

    static void Configure(UniversalRenderPipelineAsset a)
    {
        if (a == null) return;
        a.msaaSampleCount = 4;
        a.supportsHDR = true;
        a.renderScale = 1f;
        a.supportsCameraDepthTexture = true;
        a.supportsCameraOpaqueTexture = true;
        a.shadowCascadeCount = 2;
        a.shadowDistance = 30f;
        a.mainLightShadowmapResolution = 1024;
        // Read-only in the API: set the serialized fields directly.
        var so = new SerializedObject(a);
        so.FindProperty("m_SoftShadowsSupported").boolValue = true;
        so.FindProperty("m_AdditionalLightShadowsSupported").boolValue = true;
        so.FindProperty("m_AdditionalLightsRenderingMode").intValue = (int)LightRenderingMode.PerPixel;
        so.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(a);
        AddSsao(a);
    }

    // Adds URP's Screen Space Ambient Occlusion renderer feature to every renderer the asset uses.
    static void AddSsao(UniversalRenderPipelineAsset a)
    {
        var list = new SerializedObject(a).FindProperty("m_RendererDataList");
        for (int i = 0; i < list.arraySize; i++)
        {
            var rd = list.GetArrayElementAtIndex(i).objectReferenceValue as ScriptableRendererData;
            if (rd == null || rd.rendererFeatures.Any(f => f is ScreenSpaceAmbientOcclusion)) continue;
            var feature = ScriptableObject.CreateInstance<ScreenSpaceAmbientOcclusion>();
            feature.name = "ProbeSSAO";
            AssetDatabase.AddObjectToAsset(feature, rd);
            AssetDatabase.TryGetGUIDAndLocalFileIdentifier(feature, out string guid, out long localId);
            var so = new SerializedObject(rd);
            var feats = so.FindProperty("m_RendererFeatures");
            var map = so.FindProperty("m_RendererFeatureMap");
            int n = feats.arraySize;
            feats.arraySize = n + 1; map.arraySize = n + 1;
            feats.GetArrayElementAtIndex(n).objectReferenceValue = feature;
            map.GetArrayElementAtIndex(n).longValue = localId;
            so.ApplyModifiedPropertiesWithoutUndo();
            EditorUtility.SetDirty(rd);
        }
    }

    static Texture2D MakeCheckerTexture()
    {
        string path = Dir + "/checker.png";
        var t = new Texture2D(256, 256, TextureFormat.RGBA32, false);
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 256; x++)
            {
                bool c = ((x / 32) + (y / 32)) % 2 == 0;
                t.SetPixel(x, y, c ? new Color(0.9f, 0.9f, 0.9f) : new Color(0.2f + x / 512f, 0.3f, 0.2f + y / 512f));
            }
        File.WriteAllBytes(path, t.EncodeToPNG());
        AssetDatabase.ImportAsset(path);
        var imp = (TextureImporter)AssetImporter.GetAtPath(path);
        imp.mipmapEnabled = true;
        imp.textureCompression = TextureImporterCompression.Compressed; // ASTC/ETC2 on Android
        imp.SaveAndReimport();
        return AssetDatabase.LoadAssetAtPath<Texture2D>(path);
    }

    static Material Lit(string name, Color color, float metallic, float smoothness, Texture tex)
    {
        var m = Save(new Material(Shader.Find("Universal Render Pipeline/Lit")), name);
        m.SetColor("_BaseColor", color);
        m.SetFloat("_Metallic", metallic);
        m.SetFloat("_Smoothness", smoothness);
        if (tex != null) m.SetTexture("_BaseMap", tex);
        return m;
    }

    static void MakeTransparent(Material m)
    {
        m.SetFloat("_Surface", 1);
        m.SetFloat("_Blend", 0);
        m.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha);
        m.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
        m.SetFloat("_ZWrite", 0);
        m.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        m.SetOverrideTag("RenderType", "Transparent");
        m.renderQueue = (int)RenderQueue.Transparent;
    }

    static Material Save(Material m, string name)
    {
        string path = Dir + "/" + name + ".mat";
        AssetDatabase.DeleteAsset(path);
        AssetDatabase.CreateAsset(m, path);
        return m;
    }

    static Light NewLight(string name, LightType type, Vector3 euler, Vector3 pos, float intensity, Color color)
    {
        var go = new GameObject(name);
        go.transform.SetPositionAndRotation(pos, Quaternion.Euler(euler));
        var l = go.AddComponent<Light>();
        l.type = type; l.intensity = intensity; l.color = color;
        return l;
    }

    static GameObject Prim(string name, PrimitiveType type, Vector3 pos, Vector3 scale, Material mat)
    {
        var go = GameObject.CreatePrimitive(type);
        go.name = name;
        go.transform.position = pos; go.transform.localScale = scale;
        go.GetComponent<Renderer>().sharedMaterial = mat;
        return go;
    }

    static T AddOv<T>(VolumeProfile p) where T : VolumeComponent
    {
        var c = p.Add<T>(false);
        c.name = typeof(T).Name;
        AssetDatabase.AddObjectToAsset(c, p);
        return c;
    }

    static VolumeProfile MakeUrpProfile()
    {
        string path = Dir + "/ProbeUrpProfile.asset";
        AssetDatabase.DeleteAsset(path);
        var p = ScriptableObject.CreateInstance<VolumeProfile>();
        AssetDatabase.CreateAsset(p, path);
        var bloom = AddOv<UnityEngine.Rendering.Universal.Bloom>(p);
        bloom.intensity.Override(1.2f); bloom.threshold.Override(0.8f);
        AddOv<Tonemapping>(p).mode.Override(TonemappingMode.ACES);
        AddOv<UnityEngine.Rendering.Universal.Vignette>(p).intensity.Override(0.35f);
        AddOv<ColorAdjustments>(p).saturation.Override(25f);
        var dof = AddOv<UnityEngine.Rendering.Universal.DepthOfField>(p);
        dof.mode.Override(DepthOfFieldMode.Gaussian); dof.gaussianStart.Override(15f); dof.gaussianEnd.Override(40f);
        AddOv<FilmGrain>(p).intensity.Override(0.2f);
        AddOv<UnityEngine.Rendering.Universal.ChromaticAberration>(p).intensity.Override(0.15f);
        EditorUtility.SetDirty(p);
        return p;
    }

    static PostProcessProfile MakePpProfile()
    {
        string path = Dir + "/ProbePPProfile.asset";
        AssetDatabase.DeleteAsset(path);
        var p = ScriptableObject.CreateInstance<PostProcessProfile>();
        AssetDatabase.CreateAsset(p, path);
        var bloom = p.AddSettings<PPBloom>(); bloom.enabled.Override(true); bloom.intensity.Override(1.5f);
        var vig = p.AddSettings<PPVignette>(); vig.enabled.Override(true); vig.intensity.Override(0.4f);
        var cg = p.AddSettings<ColorGrading>(); cg.enabled.Override(true); cg.tonemapper.Override(Tonemapper.ACES); cg.saturation.Override(20f);
        var dof = p.AddSettings<PPDof>(); dof.enabled.Override(true); dof.focusDistance.Override(8f);
        var ca = p.AddSettings<UnityEngine.Rendering.PostProcessing.ChromaticAberration>(); ca.enabled.Override(true); ca.intensity.Override(0.2f);
        var grain = p.AddSettings<UnityEngine.Rendering.PostProcessing.Grain>(); grain.enabled.Override(true); grain.intensity.Override(0.3f);
        foreach (var s in p.settings) AssetDatabase.AddObjectToAsset(s, p);
        EditorUtility.SetDirty(p);
        return p;
    }

    // A separate high-priority volume so a broken AO mode shows up on its own stage; starts inactive.
    static GameObject MakeAoVolume(Transform parent, string name, AmbientOcclusionMode mode)
    {
        string path = Dir + "/Probe" + name.Replace(" ", "").Replace("(", "").Replace(")", "") + ".asset";
        AssetDatabase.DeleteAsset(path);
        var p = ScriptableObject.CreateInstance<PostProcessProfile>();
        AssetDatabase.CreateAsset(p, path);
        var ao = p.AddSettings<UnityEngine.Rendering.PostProcessing.AmbientOcclusion>();
        ao.enabled.Override(true);
        ao.mode.Override(mode);
        ao.intensity.Override(1.5f);
        ao.radius.Override(0.5f);
        foreach (var s in p.settings) AssetDatabase.AddObjectToAsset(s, p);
        EditorUtility.SetDirty(p);
        var go = new GameObject(name);
        go.transform.SetParent(parent, false);
        var v = go.AddComponent<PostProcessVolume>();
        v.isGlobal = true; v.priority = 10; v.sharedProfile = p;
        go.SetActive(false);
        return go;
    }

    static PostProcessResources FindPpResources()
    {
        foreach (string g in AssetDatabase.FindAssets("t:PostProcessResources"))
            return AssetDatabase.LoadAssetAtPath<PostProcessResources>(AssetDatabase.GUIDToAssetPath(g));
        throw new System.Exception("PostProcessResources asset not found (is com.unity.postprocessing installed?)");
    }
}
