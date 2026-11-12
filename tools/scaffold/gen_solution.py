"""Generate VagrantStory.Native.sln and every Native\\*.vcxproj (+ .vcxproj.filters) from one declarative table.

Layout (owner decision 2026-10-17, "hybrid"):
  Native\\Engine   VS.Engine   engine systems, one folder each: src\\<System>\\, include\\<System>\\, Legacy\\<System>\\
  Native\\Content  VS.Content  content loading (catalogs, data files): src\\, include\\Content\\, Legacy\\
  Native\\Game     VS.Game     game modules: src\\<Module>\\, include\\<Module>\\
  Native\\Desktop  VS.Desktop  the executable
  Native\\Tests    VS.Tests    unit tests
  Native\\Tools\\Extract  VS.Extract  disc -> Native\\data converter (offline tool)

Includes are written "<System or Module>/<File>.hpp" (e.g. "Core/Log.hpp", "Content/Disc.hpp", "Boot/GameApp.hpp").
Files are listed explicitly and mirrored as filters, so Solution Explorer shows the folders on disk.
Re-run after adding or removing files (the copy tool runs it after writing):
  python tools\\scaffold\\gen_solution.py
"""
from __future__ import annotations

import json
import os
import uuid
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Native"
NS = uuid.UUID("6f1c2a52-3c1e-4c55-9a43-56d0a1f0b7e1")  # deterministic GUIDs
SOURCE_ROOTS = ("src", "include", "Legacy")
CODE = {".cpp": "ClCompile", ".hpp": "ClInclude", ".h": "ClInclude", ".inl": "ClInclude"}


@dataclass
class Project:
    name: str            # VS.Engine
    path: str            # dir relative to Native\
    kind: str = "lib"    # lib | exe
    refs: list[str] = field(default_factory=list)       # project references (build order + linking)
    see: list[str] = field(default_factory=list)        # include paths of projects not referenced (avoids cycles)
    console: bool = False
    folders: list[str] = field(default_factory=list)    # per-system/module folders under src\, include\ (Legacy\)
    legacy: bool = False
    libs: list[str] = field(default_factory=list)       # vcpkg packages to link (exe only): sdl3, catch2
    whole_archive: list[str] = field(default_factory=list)  # libraries linked in full (exe only)
    extra_sources: list[str] = field(default_factory=list)  # paths relative to Native\ (shared test subjects)

    @property
    def guid(self) -> str:
        return "{" + str(uuid.uuid5(NS, self.name)).upper() + "}"

    @property
    def dir(self) -> Path:
        return NATIVE / self.path


ENGINE_SYSTEMS = ["Core", "Platform", "Render", "Audio", "Input", "Movie", "Save"]
GAME_MODULES = ["Boot", "Title", "Field", "Battle", "Actors", "Items", "Menu", "Event", "Effects", "Ending", "State"]

# Engine and Content call each other (Movie/Save read content; Content uses Core), so Engine sees Content's headers
# without referencing the project; the executables link both static libraries.
PROJECTS = [
    Project("VS.Engine", "Engine", refs=[], see=["VS.Content"], folders=ENGINE_SYSTEMS, legacy=True),
    Project("VS.Content", "Content", refs=["VS.Engine"], legacy=True),
    Project("VS.Game", "Game", refs=["VS.Engine", "VS.Content"], folders=GAME_MODULES),
    Project("VS.Desktop", "Desktop", kind="exe", refs=["VS.Engine", "VS.Content", "VS.Game"], libs=["sdl3", "movie", "audio", "disc"]),
    Project("VS.CombatLab", "Tools\\CombatLab", kind="exe",
            refs=["VS.Engine", "VS.Content", "VS.Game"], libs=["sdl3", "movie", "audio", "disc"]),
    Project("VS.Extract", "Tools\\Extract", kind="exe", console=True, refs=["VS.Engine", "VS.Content"],
            libs=["sdl3", "disc"]),
    Project("VS.Launcher", "Tools\\Launcher", kind="exe",
            refs=["VS.Engine", "VS.Content"], libs=["sdl3", "shell", "crypto", "http", "disc"]),
    Project("VS.Tests", "Tests", kind="exe", console=True, refs=["VS.Engine", "VS.Content", "VS.Game"],
            libs=["sdl3", "catch2", "movie", "audio", "crypto", "http", "disc"],
            whole_archive=["VS.Engine", "VS.Content", "VS.Game"],
            see=["VS.CombatLab", "VS.Launcher"], extra_sources=[
                r"Tools\Launcher\src\Setup.cpp",
                r"Tools\Launcher\src\DiscValidation.cpp",
                r"Tools\Launcher\src\Updates.cpp",
                r"Tools\CombatLab\src\Canvas.cpp",
                r"Tools\CombatLab\src\DoorTests.cpp",
                r"Tools\CombatLab\src\Editor.cpp",
                r"Tools\CombatLab\src\SceneCatalog.cpp",
                r"Tools\CombatLab\src\Trace.cpp"]),
]
BY_NAME = {p.name: p for p in PROJECTS}

# Import libraries per vcpkg package (vcpkg's lib\*.lib autolink does not reach the link line here).
VCPKG_LIBS = {
    "sdl3": {"Debug": ["SDL3.lib"], "Release": ["SDL3.lib"]},
    "shell": {"Debug": ["ole32.lib", "shell32.lib", "uuid.lib", "comctl32.lib", "uxtheme.lib"],
              "Release": ["ole32.lib", "shell32.lib", "uuid.lib", "comctl32.lib", "uxtheme.lib"]},
    "crypto": {"Debug": ["bcrypt.lib"], "Release": ["bcrypt.lib"]},
    "http": {"Debug": ["winhttp.lib"], "Release": ["winhttp.lib"]},
    "audio": {"Debug": ["sfizz.lib"], "Release": ["sfizz.lib"]},
    "catch2": {"Debug": ["Catch2d.lib"], "Release": ["Catch2.lib"]},
    # Engine\Movie: AV1 decoding and Matroska demuxing (dr_flac is header-only)
    "movie": {"Debug": ["dav1d.lib", "webm.lib"], "Release": ["dav1d.lib", "webm.lib"]},
    # Content\\Disc: CHD disc images
    "disc": {"Debug": ["chdr.lib"], "Release": ["chdr.lib"]},
}
VCPKG_PROPS = r"$(VCInstallDir)vcpkg\scripts\buildsystems\msbuild\vcpkg.props"
VCPKG_TARGETS = r"$(VCInstallDir)vcpkg\scripts\buildsystems\msbuild\vcpkg.targets"


def rel(frm: Path, to: Path) -> str:
    return os.path.relpath(to, frm).replace("/", "\\")


def guid_of(key: str) -> str:
    return "{" + str(uuid.uuid5(NS, key)).upper() + "}"


def files_of(p: Project) -> list[tuple[str, str]]:
    """(item type, path relative to the project) for every source file on disk, in path order."""
    out = []
    for r in SOURCE_ROOTS:
        base = p.dir / r
        if base.is_dir():
            for f in sorted(base.rglob("*")):
                if f.is_file() and f.suffix in CODE:
                    out.append((CODE[f.suffix], rel(p.dir, f)))
    for source in p.extra_sources:
        out.append((CODE[Path(source).suffix], rel(p.dir, NATIVE / source)))
    return out


def copied_settings_sources() -> frozenset[str]:
    """Sources compiled with the copied-code settings: everything the copy tool writes, plus hand-maintained sources
    not yet promoted (tools\\port\\relaxed_sources.txt). Promotion removes a file from the list (plan §8.2)."""
    port = ROOT / "tools" / "port"
    generated = json.loads((port / "copy_manifest.json").read_text(encoding="utf-8"))
    listed = (port / "relaxed_sources.txt").read_text(encoding="utf-8").split()
    return frozenset(path for path in (*generated, *listed) if path.endswith(".cpp"))


COPIED_SETTINGS = copied_settings_sources()


def uses_copied_settings(p: Project, path: str) -> bool:
    return rel(ROOT, p.dir / path) in COPIED_SETTINGS


def needs_c_headers(project: str, path: str) -> bool:
    path = path.replace("/", "\\")
    return (uses_copied_settings(BY_NAME[project], path) or "\\Legacy\\" in "\\" + path
            or project == "VS.Engine" and path in (
                r"src\Audio\Driver\VoiceOutputLive.cpp",
                r"src\Audio\Cutscene\StreamControlLive.cpp",
                r"src\Audio\XaPlayback.cpp",
                r"src\Audio\MusicLayers.cpp",
                r"src\Render\ScaledFontLive.cpp",
                r"src\Render\TextureRegionDrawLive.cpp",
                r"src\Render\SpritesLive.cpp",
                r"src\Render\ModelGeometryLive.cpp",
                r"src\Render\RoomDrawLive.cpp",
                r"src\Render\CameraViewLive.cpp",
                r"src\Render\DisplayControlLive.cpp",
                r"src\Render\DepthCueLive.cpp",
                r"src\Render\UiPacketsLive.cpp",
                r"src\Render\TexturePagesLive.cpp",
                r"src\Render\TimImageLive.cpp",
                r"src\Render\ScreenOverlayLive.cpp",
                r"src\Render\PaletteAnimationLive.cpp",
                r"src\Render\GimImagesLive.cpp",
                r"src\Render\EncodedTextLive.cpp",
                r"src\Render\GameFontLive.cpp",
                r"src\Audio\SoundEffectsLive.cpp",
                r"src\Movie\Intro\PlayerLive.cpp",
                r"src\Audio\Driver\PlaybackResetLive.cpp",
                r"src\Core\RandomLive.cpp",
                r"src\Input\ControllerLive.cpp",
                r"src\Audio\Seq\SeqVoiceCommandsLive.cpp",
                r"src\Audio\Seq\SeqRecordsLive.cpp",
                r"src\Audio\Seq\SeqControlCommandsLive.cpp",
                r"src\Audio\Seq\SeqMusicSequenceLive.cpp",
                r"src\Audio\Seq\SeqSequencerLive.cpp",
                r"src\Audio\Driver\SoundDriverLive.cpp",
                r"src\Audio\Mixer\VoiceMixerLive.cpp",
                r"src\Audio\Mixer\MixControlLive.cpp",
                r"src\Audio\Playback\SoundPlaybackLive.cpp",
                r"src\Audio\Samples\SampleBanksLive.cpp",
                r"src\Audio\SoundBanksLive.cpp",
                r"src\Audio\Control\SoundControlLive.cpp",
                r"src\Audio\SoundEffects\SoundEffectProgramsLive.cpp",
                r"src\Audio\SfxLive.cpp",
                r"src\Audio\SoundSlotsLive.cpp",
                r"src\Audio\MusicLive.cpp",
                r"src\Audio\StreamLive.cpp",
                r"src\Audio\Cutscene\StreamBuffersLive.cpp",
                r"src\Audio\ActorSoundLive.cpp",
                r"src\Audio\AmbienceLive.cpp",
            )
            or project == "VS.Content" and path in (
                r"src\DiscReaderLive.cpp",
                r"src\ActorAssetsLive.cpp",
                r"src\CdDriveLive.cpp",
                r"src\OverlayWorkspaceLive.cpp",
                r"src\PFileLive.cpp",
                r"src\SystemResourcesLive.cpp",
            )
            or project == "VS.Tests"
            or project == "VS.CombatLab"
            or project == "VS.Game" and path in (
                r"src\Boot\Encounter.cpp",
                r"src\Diagnostics\PlayerObservation.cpp",
                r"src\Battle\SphereTerrainLive.cpp",
                r"src\Battle\TargetPagesLive.cpp",
                r"src\Battle\Ai\PlanningWorkspaceLive.cpp",
                r"src\Battle\Ai\FrameUpdateLive.cpp",
                r"src\Battle\Ai\CurrentTerrain.cpp",
                r"src\Field\EncounterCatalog.cpp",
                r"src\Field\SegmentCollisionLive.cpp",
                r"src\Field\PlatformPlacementLive.cpp",
                r"src\Field\RoomTriggerMarkersLive.cpp",
                r"src\Field\RoomLightingLive.cpp",
                r"src\Field\MinimapSceneLive.cpp",
                r"src\Battle\Ai\PlanningPrimitivesLive.cpp",
                r"src\Battle\Ai\DecisionPrimitivesLive.cpp",
                r"src\Battle\Ai\NavigationPrimitivesLive.cpp",
                r"src\Battle\Ai\ActorCommandsLive.cpp",
                r"src\Battle\Ai\RelationsLive.cpp",
                r"src\Battle\Ai\ActorSettingsLive.cpp",
                r"src\Battle\Ai\ActorCreationLive.cpp",
                r"src\Battle\Ai\DecisionsLive.cpp",
                r"src\Actors\AppearanceTransitionLive.cpp",
                r"src\Save\NativeContainerLive.cpp",
                r"src\Actors\SequencePlaybackLive.cpp",
                r"src\Actors\ModelDrawingLive.cpp",
                r"src\Save\MenuAssetsLive.cpp",
                r"src\Boot\SessionLive.cpp",
                r"src\Boot\GameplayOverlayLive.cpp",
                r"src\Boot\GameplayLoopLive.cpp",
                r"src\Field\InteractionsLive.cpp",
                r"src\Field\InteractionTargetLive.cpp",
                r"src\Field\ChestInteractionsLive.cpp",
                r"src\Field\GimmickPlacementLive.cpp",
                r"src\Field\GimmickResourcesLive.cpp",
                r"src\Field\GimmickReactionsLive.cpp",
                r"src\Field\GimmickDynamicsLive.cpp",
                r"src\Field\RoomSwitchesLive.cpp",
                r"src\Field\GimmickAnimationQueueLive.cpp",
                r"src\Event\TriggersLive.cpp",
                r"src\Battle\Ai\EngagementLive.cpp",
                r"src\Battle\Ai\ActorCleanupLive.cpp",
                r"src\Battle\Ai\MovementBehaviorLive.cpp",
                r"src\Actors\MotionRequestsLive.cpp",
                r"src\Actors\AnimationPoseLive.cpp",
                r"src\Ending\Credits\EmbeddedArt.cpp",
                r"src\Actors\SoundTransitionBinding.cpp",
                r"src\Actors\MovementCommandBinding.cpp",
                r"src\Battle\Ai\ConditionSnapshotBinding.cpp",
                r"src\Field\CollisionSamplingBinding.cpp",
                r"src\Event\WeaponModeLive.cpp",
                r"src\Event\ScriptActorCommandsLive.cpp",
                r"src\Event\ScriptAudioCommandsLive.cpp",
                r"src\Event\ScriptDialogCommandsLive.cpp",
                r"src\Event\ScriptEffectCommandsLive.cpp",
                r"src\Event\CutsceneLifecycleLive.cpp",
                r"src\Event\ScriptVmLive.cpp",
                r"src\Effects\ScreenEffectState.cpp",
                r"src\Effects\ScreenDistortion\RadialWarpLive.cpp",
                r"src\Battle\TargetingDrawLive.cpp",
                r"src\Battle\ActionResultsLive.cpp",
                r"src\Field\DoorMatchesLive.cpp",
                r"src\Effects\ParticleRenderLive.cpp",
                r"src\Effects\PackageExtentsLive.cpp",
                r"src\Effects\TrailEmitterLive.cpp",
                r"src\Effects\Templates\RingsLive.cpp",
                r"src\Effects\Templates\ParticlesLive.cpp",
                r"src\Effects\Templates\TrailsLive.cpp",
                r"src\Effects\Templates\SurfacesLive.cpp",
                r"src\Menu\Framework\FrameworkLive.cpp",
                r"src\Menu\FilesLive.cpp",
                r"src\Menu\OptionsLive.cpp",
                r"src\Menu\RenameLive.cpp",
                r"src\Menu\TextBoxFramesLive.cpp",
                r"src\Menu\Framework\ItemRowsLive.cpp",
                r"src\Menu\Framework\EquipmentDetailsLive.cpp",
                r"src\Menu\Framework\ActorPreviewLive.cpp",
                r"src\Menu\Setup\SetupLive.cpp",
                r"src\Effects\TextureBindingsLive.cpp",
                r"src\Battle\Ai\GroundNavigationLive.cpp",
                r"src\Battle\Ai\SpecialTerrainLive.cpp",
                r"src\Battle\Ai\RoomAiMapLive.cpp",
                r"src\Battle\Ai\AreaRouteLive.cpp",
                r"src\Battle\Ai\WaypointsLive.cpp",
                r"src\Battle\Ai\WanderLive.cpp",
                r"src\Battle\Ai\RadialMovementLive.cpp",
                r"src\Battle\Ai\BlockedMovementLive.cpp",
                r"src\Battle\Ai\FlightMovementLive.cpp",
                r"src\Battle\Ai\TargetApproachLive.cpp",
                r"src\Battle\Ai\BossOrbitLive.cpp",
                r"src\Battle\Ai\ActorPerceptionLive.cpp",
                r"src\Battle\Ai\CandidateMovementLive.cpp",
                r"src\Battle\Ai\ActionExecutionLive.cpp",
                r"src\Save\NativeSnapshot.cpp",
                r"src\Save\RuntimeSnapshot.cpp",
                r"src\Save\NativeFiles.cpp",
                r"src\Save\NativeResume.cpp",
                r"src\Save\FileMenuLive.cpp",
                r"src\Save\NativeFilePresentation.cpp",
                r"src\Save\MemoryCard\GameFilesLive.cpp",
                r"src\Save\SnapshotLive.cpp",
                r"src\Items\ItemRecordsLive.cpp",
                r"src\Items\EquipmentLive.cpp",
                r"src\Items\LootLive.cpp",
                r"src\Items\InventoryLive.cpp",
                r"src\Actors\ActorTableLive.cpp",
                r"src\Actors\AppearanceLive.cpp",
                r"src\Actors\TextureAnimationLive.cpp",
                r"src\Actors\ModelRenderingLive.cpp",
                r"src\Actors\SkeletonLive.cpp",
                r"src\Actors\ActorMotionLive.cpp",
                r"src\Actors\ActorCommandsLive.cpp",
                r"src\Actors\ActorMovementLive.cpp",
                r"src\Actors\AnimationControlLive.cpp",
                r"src\State\ScriptFlagsLive.cpp",
                r"src\State\SceneTimerLive.cpp",
                r"src\State\GameClockLive.cpp",
                r"src\State\KillScoreLive.cpp",
                r"src\Field\CollisionMapLive.cpp",
                r"src\Field\MapLive.cpp",
                r"src\Field\TrapsLive.cpp",
                r"src\Field\ActorCollisionLive.cpp",
                r"src\Field\RoomEffectsLive.cpp",
                r"src\Actors\RoomEnemyMemoryLive.cpp",
                r"src\Actors\ActorRosterLive.cpp",
                r"src\Actors\ActorStateLive.cpp",
                r"src\Actors\ActorSpawningLive.cpp",
                r"src\Ending\EndingLive.cpp",
                r"src\Effects\CurvesLive.cpp",
                r"src\Effects\CameraLive.cpp",
                r"src\Effects\TransformsLive.cpp",
                r"src\Effects\EffectResourcesLive.cpp",
                r"src\Title\TitleMenuLive.cpp",
                r"src\Field\CameraLive.cpp",
                r"src\Field\BackgroundLive.cpp",
                r"src\Field\EnvironmentLive.cpp",
                r"src\Field\DoorRecordLive.cpp",
                r"src\Field\TransitionsLive.cpp",
                r"src\Field\RoomStateLive.cpp",
                r"src\Field\GameModeLive.cpp",
                r"src\Field\RoomRenderingLive.cpp",
                r"src\Field\RoomLoadingLive.cpp",
                r"src\Battle\TargetingLive.cpp",
                r"src\Battle\AbilitiesLive.cpp",
                r"src\Battle\AttackPrerequisitesLive.cpp",
                r"src\Battle\BattleModeLive.cpp",
                r"src\Battle\StatusQueueLive.cpp",
                r"src\Battle\StatusHelpersLive.cpp",
            ))


def vcxproj(p: Project, files: list[tuple[str, str]]) -> str:
    ctype = "Application" if p.kind == "exe" else "StaticLibrary"
    inc_dirs = ["$(MSBuildThisFileDirectory)include", "$(MSBuildThisFileDirectory)src"] + [
        f"$(MSBuildThisFileDirectory){rel(p.dir, BY_NAME[r].dir)}\\include" for r in p.refs + p.see]
    if p.name == "VS.Tests":
        inc_dirs.append("$(VSCopiedCIncludes)")  # tests exercise the Legacy layer and copied headers
    compile_items, header_items = [], []
    for kind, path in files:
        if kind == "ClInclude":
            header_items.append(f'    <ClInclude Include="{path}" />')
        elif uses_copied_settings(p, path):
            # Copied decomp code gets a relaxed warning set until promoted (plan Â§8.2).
            compile_items.append(f'    <ClCompile Include="{path}">\n'
                                 f'      <TreatWarningAsError>false</TreatWarningAsError>\n'
                                 f'      <WarningLevel>Level1</WarningLevel>\n'
                                 f'      <ConformanceMode>false</ConformanceMode>\n'
                                 f'      <AdditionalIncludeDirectories>$(VSCopiedCIncludes);%(AdditionalIncludeDirectories)'
                                 f'</AdditionalIncludeDirectories>\n'
                                 f'      <AdditionalOptions>$(VSCopiedCOptions) %(AdditionalOptions)</AdditionalOptions>\n'
                                 f'    </ClCompile>')
        elif needs_c_headers(p.name, path) and p.name != "VS.Tests":
            # Legacy shims include the C SDK headers of other projects.
            compile_items.append(f'    <ClCompile Include="{path}">\n'
                                 f'      <AdditionalIncludeDirectories>$(VSCopiedCIncludes);%(AdditionalIncludeDirectories)'
                                 f'</AdditionalIncludeDirectories>\n'
                                 f'    </ClCompile>')
        else:
            compile_items.append(f'    <ClCompile Include="{path}" />')
    refs = "\n".join(
        f'    <ProjectReference Include="{rel(p.dir, BY_NAME[r].dir)}\\{r}.vcxproj">\n'
        f'      <Project>{BY_NAME[r].guid}</Project>\n    </ProjectReference>' for r in p.refs)
    link = ""
    if p.kind == "exe":
        sub = "Console" if p.console else "Windows"
        deps = ""
        for cfg in ("Debug", "Release"):
            libs = [lib for pkg in p.libs for lib in VCPKG_LIBS[pkg][cfg]]
            if libs:
                deps += (f"      <AdditionalDependencies Condition=\"'$(Configuration)'=='{cfg}'\">"
                         f"{';'.join(libs)};%(AdditionalDependencies)</AdditionalDependencies>\n")
        whole = ""
        if p.whole_archive:
            # link every object of these libraries, so an unresolved or duplicate symbol in copied code fails the
            # build even before anything calls it
            whole = ("      <AdditionalOptions>" +
                     " ".join(f"/WHOLEARCHIVE:$(OutDir){lib}.lib" for lib in p.whole_archive) +
                     " %(AdditionalOptions)</AdditionalOptions>\n")
        manifests = "$(VSNative)Windows.manifest;"
        if p.name == "VS.Launcher":
            manifests += "$(MSBuildThisFileDirectory)Launcher.manifest;"
        link = (f"    <Link>\n      <SubSystem>{sub}</SubSystem>\n{deps}{whole}    </Link>\n"
                "    <Manifest>\n"
                f"      <AdditionalManifestFiles>{manifests}"
                "%(AdditionalManifestFiles)</AdditionalManifestFiles>\n"
                "    </Manifest>\n")
    configs = ""
    for cfg in ("Debug", "Release"):
        dbg = "true" if cfg == "Debug" else "false"
        configs += f"""  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='{cfg}|x64'" Label="Configuration">
    <ConfigurationType>{ctype}</ConfigurationType>
    <UseDebugLibraries>{dbg}</UseDebugLibraries>
    <PlatformToolset>ClangCL</PlatformToolset>
  </PropertyGroup>
"""
    groups = ""
    for items in (compile_items, header_items):
        if items:
            groups += "  <ItemGroup>\n" + "\n".join(items) + "\n  </ItemGroup>\n"
    if refs:
        groups += f"  <ItemGroup>\n{refs}\n  </ItemGroup>\n"
    desktop_policy = (
        "      <PreprocessorDefinitions Condition=\"'$(Configuration)'=='Release'\">"
        "VS_RELEASE_BUILD;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
        if p.name == "VS.Desktop" else ""
    )
    return f"""<?xml version="1.0" encoding="utf-8"?>
<!-- GENERATED by tools\\scaffold\\gen_solution.py. Edit the table there, not this file. -->
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <ProjectGuid>{p.guid}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>{p.name.replace('.', '')}</RootNamespace>
    <ProjectName>{p.name}</ProjectName>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <PreferredToolArchitecture>x64</PreferredToolArchitecture>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
{configs}  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <Import Project="{VCPKG_PROPS}" Condition="Exists('{VCPKG_PROPS}')" />
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>{';'.join(inc_dirs)};%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
{desktop_policy}    </ClCompile>
{link}  </ItemDefinitionGroup>
{groups}  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <Import Project="{VCPKG_TARGETS}" Condition="Exists('{VCPKG_TARGETS}')" />
</Project>
"""


def filters(p: Project, files: list[tuple[str, str]]) -> str:
    """One filter per folder on disk, so Solution Explorer mirrors src\\, include\\ and Legacy\\."""
    dirs: set[str] = set()
    for _, path in files:
        parts = path.split("\\")[:-1]
        for i in range(1, len(parts) + 1):
            dirs.add("\\".join(parts[:i]))
    flt = "\n".join(f'    <Filter Include="{d}">\n      <UniqueIdentifier>{guid_of(p.name + ":" + d)}'
                    f'</UniqueIdentifier>\n    </Filter>' for d in sorted(dirs))
    items = []
    for kind, path in files:
        folder = "\\".join(path.split("\\")[:-1])
        items.append(f'    <{kind} Include="{path}">\n      <Filter>{folder}</Filter>\n    </{kind}>'
                     if folder else f'    <{kind} Include="{path}" />')
    body = ""
    if flt:
        body += f"  <ItemGroup>\n{flt}\n  </ItemGroup>\n"
    if items:
        body += "  <ItemGroup>\n" + "\n".join(items) + "\n  </ItemGroup>\n"
    return f"""<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
{body}</Project>
"""


FOLDER_GUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
CPP_GUID = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
# Solution Items are discovered (tracked or new, not ignored): the repository's root files, and every file under
# these folders as nested solution folders (Solution Items\Docs\ledgers, ...).
SOLUTION_ITEM_DIRS = ["Docs", ".github"]


def solution_item_files() -> list[str]:
    import subprocess
    listed = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=ROOT,
                            capture_output=True, text=True).stdout.split("\0")
    out = []
    for f in sorted({t.replace("/", "\\") for t in listed if t}):
        top = f.split("\\")[0]
        if ((("\\" not in f and not f.endswith(".sln")) or top in SOLUTION_ITEM_DIRS)
                and (ROOT / f).is_file()):
            out.append(f)
    return out

# Tool folders shown in Solution Explorer (no build). The repository's tools\ and the machine's external tool root
# (C:\Tools, or the VS_EXTERNAL_TOOLS environment variable) hold multi-GB toolchain and emulator trees, so a folder
# that is not tracked by git contributes only its entry files near the top (executables, scripts, docs, configs).
ITEM_DIR = ROOT / "tools" / "scaffold" / "projects"
ENTRY_EXT = {".exe", ".md", ".txt", ".ps1", ".py", ".cmd", ".bat", ".ini", ".json", ".toml", ".yaml", ".yml", ".cfg"}
ENTRY_DEPTH = 4


@dataclass
class ItemProject:
    name: str          # project (and Solution Explorer) name
    root: Path         # folder shown
    label: str         # top filter, e.g. "tools" or "C:\Tools"
    ext: frozenset[str] = frozenset(ENTRY_EXT)
    skip: tuple[str, ...] = ()   # folder names (fnmatch) not shown
    depth: int = ENTRY_DEPTH

    @property
    def guid(self) -> str:
        return guid_of("items:" + self.name)

    @property
    def external(self) -> bool:
        return not self.root.is_relative_to(ROOT)

    @property
    def path(self) -> Path:
        return ITEM_DIR / f"{self.name}.vcxproj"

    @property
    def present(self) -> bool:
        """In the solution: the folder exists here, or another machine generated the project (it loads empty)."""
        return self.root.is_dir() or self.path.exists()


def external_tools_root() -> Path:
    return Path(os.environ.get("VS_EXTERNAL_TOOLS", r"C:\Tools"))


def workshop_root() -> Path:
    return Path(os.environ.get("VS_WORKSHOP", r"D:\Dev\Modding\VS-Workshop"))


# Machine-specific folders (C:\Tools, VS-Workshop) are optional: the project lists their files only when the folder
# exists (MSBuild Exists condition), and a clone without them neither breaks nor gains a project.
ITEM_PROJECTS = [ItemProject("tools", ROOT / "tools", "tools"),
                 ItemProject("C-Tools", external_tools_root(), "C-Tools"),
                 ItemProject("VS-Workshop", workshop_root(), "VS-Workshop",
                             ext=frozenset(ENTRY_EXT | {".cs", ".xaml", ".csproj", ".sln", ".props", ".targets",
                                                        ".http", ".manifest"}),
                             skip=("bin", "obj", ".vs", ".git", "build*", "workspace", "node_modules"), depth=8)]


def entry_files(root: Path, ext=frozenset(ENTRY_EXT), skip: tuple[str, ...] = (), max_depth: int = ENTRY_DEPTH) -> list[Path]:
    import fnmatch
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        depth = len(Path(dirpath).relative_to(root).parts)
        dirnames[:] = sorted(d for d in dirnames if d not in ("__pycache__", ".git", "node_modules")
                             and not any(fnmatch.fnmatch(d, s) for s in skip)) \
            if depth < max_depth else []
        out += [Path(dirpath) / f for f in sorted(filenames) if Path(f).suffix.lower() in ext]
    return out


def item_files(ip: ItemProject) -> list[Path]:
    if not ip.root.is_dir():
        return []
    if ip.root.is_relative_to(ROOT):
        import subprocess
        tracked = subprocess.run(["git", "ls-files", "-z", "--", str(ip.root.relative_to(ROOT))], cwd=ROOT,
                                 capture_output=True, text=True).stdout.split("\0")
        files = {ROOT / t for t in tracked if t and "__pycache__" not in t}
        for d in ip.root.iterdir():   # untracked mirrors (tools\Base, tools\PSX): entry files only
            if d.is_dir() and not any(f.is_relative_to(d) for f in files):
                files |= set(entry_files(d))
        files = {f for f in files if not f.is_relative_to(ITEM_DIR)}
        return sorted(files)
    return entry_files(ip.root, ip.ext, ip.skip, ip.depth)


def item_project(ip: ItemProject, files: list[Path]) -> tuple[str, str]:
    def inc(f: Path) -> str:
        return os.path.relpath(f, ITEM_DIR) if f.is_relative_to(ROOT) else str(f)

    def folder(f: Path) -> str:
        return "\\".join((ip.label,) + f.relative_to(ip.root).parts[:-1])

    items = "\n".join(f'    <None Include="{inc(f)}" />' for f in files)
    condition = f" Condition=\"Exists('{ip.root}')\"" if ip.external else ""
    proj = f"""<?xml version="1.0" encoding="utf-8"?>
<!-- GENERATED by tools\\scaffold\\gen_solution.py: the {ip.label} folder ({ip.root}) for browsing. Builds nothing.{
    " Optional: lists nothing on a machine without that folder." if ip.external else ""} -->
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <ProjectGuid>{ip.guid}</ProjectGuid>
    <ProjectName>{ip.name}</ProjectName>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Utility</ConfigurationType>
    <PlatformToolset>ClangCL</PlatformToolset>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ItemGroup{condition}>
{items}
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
"""
    dirs: set[str] = set()
    for f in files:
        parts = folder(f).split("\\")
        for i in range(1, len(parts) + 1):
            dirs.add("\\".join(parts[:i]))
    flt = "\n".join(f'    <Filter Include="{d}">\n      <UniqueIdentifier>{guid_of(ip.name + ":" + d)}'
                    f'</UniqueIdentifier>\n    </Filter>' for d in sorted(dirs))
    entries = "\n".join(f'    <None Include="{inc(f)}">\n      <Filter>{folder(f)}</Filter>\n    </None>'
                        for f in files)
    filt = f"""<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
{flt}
  </ItemGroup>
  <ItemGroup{condition}>
{entries}
  </ItemGroup>
</Project>
"""
    return proj, filt


def solution() -> str:
    present = [ip for ip in ITEM_PROJECTS if ip.present]
    lines = ["", "Microsoft Visual Studio Solution File, Format Version 12.00",
             "# Visual Studio Version 18", "VisualStudioVersion = 18.0.0.0",
             "MinimumVisualStudioVersion = 10.0.40219.1"]
    ordered = [BY_NAME["VS.Desktop"]] + [p for p in PROJECTS if p.name != "VS.Desktop"]  # default startup
    for p in ordered:
        path = rel(ROOT, p.dir / f"{p.name}.vcxproj")
        lines += [f'Project("{CPP_GUID}") = "{p.name}", "{path}", "{p.guid}"', "EndProject"]
    by_dir: dict[str, list[str]] = {}
    for f in solution_item_files():
        by_dir.setdefault(f.rpartition("\\")[0], []).append(f)
    for d in list(by_dir):   # parents of nested folders, even without files of their own
        while "\\" in d:
            d = d.rpartition("\\")[0]
            by_dir.setdefault(d, [])
    by_dir.setdefault("", [])

    def item_folder(d: str) -> str:
        return guid_of("folder:Solution Items" + ("\\" + d if d else ""))

    item_nesting = []
    for d in sorted(by_dir):
        name = d.rpartition("\\")[2] if d else "Solution Items"
        lines += [f'Project("{FOLDER_GUID}") = "{name}", "{name}", "{item_folder(d)}"']
        if by_dir[d]:
            lines += ["\tProjectSection(SolutionItems) = preProject"]
            lines += [f"\t\t{i} = {i}" for i in by_dir[d]]
            lines += ["\tEndProjectSection"]
        lines += ["EndProject"]
        if d:
            item_nesting.append(f"\t\t{item_folder(d)} = {item_folder(d.rpartition(chr(92))[0])}")
    tools_folder = guid_of("folder:Tools")
    lines += [f'Project("{FOLDER_GUID}") = "Tools", "Tools", "{tools_folder}"', "EndProject"]
    for ip in present:
        path = rel(ROOT, ITEM_DIR / f"{ip.name}.vcxproj")
        lines += [f'Project("{CPP_GUID}") = "{ip.name}", "{path}", "{ip.guid}"', "EndProject"]
    lines += ["Global", "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
              "\t\tDebug|x64 = Debug|x64", "\t\tRelease|x64 = Release|x64", "\tEndGlobalSection",
              "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution"]
    for p in ordered:
        for c in ("Debug", "Release"):
            lines += [f"\t\t{p.guid}.{c}|x64.ActiveCfg = {c}|x64", f"\t\t{p.guid}.{c}|x64.Build.0 = {c}|x64"]
    for ip in present:   # not built
        lines += [f"\t\t{ip.guid}.{c}|x64.ActiveCfg = {c}|x64" for c in ("Debug", "Release")]
    lines += ["\tEndGlobalSection", "\tGlobalSection(SolutionProperties) = preSolution",
              "\t\tHideSolutionNode = FALSE", "\tEndGlobalSection",
              "\tGlobalSection(NestedProjects) = preSolution"]
    lines += [f"\t\t{ip.guid} = {tools_folder}" for ip in present]
    lines += [f"\t\t{BY_NAME['VS.Extract'].guid} = {tools_folder}"]
    lines += item_nesting
    lines += ["\tEndGlobalSection", "EndGlobal", ""]
    return "\r\n".join(lines)


def ensure_dirs() -> None:
    for p in PROJECTS:
        (p.dir / "src").mkdir(parents=True, exist_ok=True)
        (p.dir / "include").mkdir(parents=True, exist_ok=True)
        for f in p.folders:
            for d in ("src", "include") + (("Legacy",) if p.legacy else ()):
                (p.dir / d / f).mkdir(parents=True, exist_ok=True)
        if p.legacy and not p.folders:
            (p.dir / "Legacy").mkdir(parents=True, exist_ok=True)


def main() -> int:
    ensure_dirs()
    for p in PROJECTS:
        files = files_of(p)
        (p.dir / f"{p.name}.vcxproj").write_text(vcxproj(p, files), encoding="utf-8", newline="\n")
        (p.dir / f"{p.name}.vcxproj.filters").write_text(filters(p, files), encoding="utf-8", newline="\n")
    (ROOT / "VagrantStory.Native.sln").write_text(solution(), encoding="utf-8-sig", newline="")
    ITEM_DIR.mkdir(parents=True, exist_ok=True)
    for ip in ITEM_PROJECTS:
        if not ip.root.is_dir():   # optional folder missing here: keep whatever project is committed
            print(f"  {ip.name}: {ip.root} not found, " + ("project left as is" if ip.path.exists() else "skipped"))
            continue
        files = item_files(ip)
        proj, filt = item_project(ip, files)
        ip.path.write_text(proj, encoding="utf-8", newline="\n")
        (ITEM_DIR / f"{ip.name}.vcxproj.filters").write_text(filt, encoding="utf-8", newline="\n")
        print(f"  {ip.name}: {len(files)} files from {ip.root}")
    print(f"wrote VagrantStory.Native.sln and {len(PROJECTS)} projects")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
