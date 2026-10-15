"""Generate VagrantStory.Native.sln and every Native\\*.vcxproj from one declarative table.

Projects compile source via wildcards (src\\**\\*.cpp / *.c), so files written by the copy tool
are picked up without regenerating. Re-run this script only when adding/removing projects or
changing references:  python tools\\scaffold\\gen_solution.py
"""
from __future__ import annotations

import os
import uuid
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Native"
NS = uuid.UUID("6f1c2a52-3c1e-4c55-9a43-56d0a1f0b7e1")  # deterministic GUIDs


@dataclass
class Project:
    name: str            # VS.Core
    folder: str          # solution folder
    path: str            # dir relative to Native\
    kind: str = "lib"    # lib | exe
    refs: list[str] = field(default_factory=list)
    console: bool = False
    extra_dirs: list[str] = field(default_factory=list)  # extra source roots (e.g. Legacy)
    libs: list[str] = field(default_factory=list)  # vcpkg packages to link (exe only): sdl3, catch2

    @property
    def guid(self) -> str:
        return "{" + str(uuid.uuid5(NS, self.name)).upper() + "}"

    @property
    def dir(self) -> Path:
        return NATIVE / self.path


ALL_SYSTEMS = ["VS.Core", "VS.Platform", "VS.Render", "VS.Audio", "VS.Movie", "VS.Input", "VS.Content", "VS.Save"]

# Dependency direction: Core <- Platform <- {Graphics, Audio, Video, Input, Content, Save} <- Game <- Desktop
PROJECTS = [
    Project("VS.Core", "Core", "Core"),
    Project("VS.Platform", "Platform", "Platform", refs=["VS.Core"]),
    Project("VS.Render", "Graphics", "Graphics", refs=["VS.Core", "VS.Platform"], extra_dirs=["Legacy"]),
    Project("VS.Audio", "Audio", "Audio", refs=["VS.Core", "VS.Platform"], extra_dirs=["Legacy"]),
    Project("VS.Content", "Content", "Content", refs=["VS.Core", "VS.Platform"], extra_dirs=["Legacy"]),
    Project("VS.Input", "Input", "Input", refs=["VS.Core", "VS.Platform"]),
    Project("VS.Movie", "Video", "Video", refs=["VS.Core", "VS.Platform", "VS.Render", "VS.Audio", "VS.Content"],
            extra_dirs=["Legacy"]),
    Project("VS.Save", "Save", "Save", refs=["VS.Core", "VS.Platform", "VS.Content"], extra_dirs=["Legacy"]),
    Project("VS.Game", "Game", "Game", refs=ALL_SYSTEMS),
    Project("VS.Triage", "Triage", "Triage", refs=ALL_SYSTEMS + ["VS.Game"]),
    Project("VS.Desktop", "Desktop", "Desktop", kind="exe", refs=ALL_SYSTEMS + ["VS.Game", "VS.Triage"],
            libs=["sdl3"]),
    Project("VS.Extract", "Tools", "Tools\\Extract", kind="exe", console=True,
            refs=["VS.Core", "VS.Platform", "VS.Content"], libs=["sdl3"]),
    Project("VS.Tests", "Tests", "Tests", kind="exe", console=True, refs=ALL_SYSTEMS + ["VS.Game"],
            libs=["sdl3", "catch2"]),
]
BY_NAME = {p.name: p for p in PROJECTS}

# Import libraries per vcpkg package (vcpkg's lib\*.lib autolink does not reach the link line here).
VCPKG_LIBS = {
    "sdl3": {"Debug": ["SDL3.lib"], "Release": ["SDL3.lib"]},
    "catch2": {"Debug": ["Catch2d.lib"], "Release": ["Catch2.lib"]},
}
BY_NAME = {p.name: p for p in PROJECTS}

# Game modules (plan §6.8). Each gets src\<Module>\ and include\vs\game\<module>\.
GAME_MODULES = ["Boot", "Title", "Field", "Battle", "Actors", "Items", "Menu", "Event", "Effects", "Ending", "State"]
VCPKG_PROPS = r"$(VCInstallDir)vcpkg\scripts\buildsystems\msbuild\vcpkg.props"
VCPKG_TARGETS = r"$(VCInstallDir)vcpkg\scripts\buildsystems\msbuild\vcpkg.targets"


def rel(frm: Path, to: Path) -> str:
    return os.path.relpath(to, frm).replace("/", "\\")


def vcxproj(p: Project) -> str:
    ctype = "Application" if p.kind == "exe" else "StaticLibrary"
    roots = ["src"] + p.extra_dirs
    inc_dirs = ["$(MSBuildThisFileDirectory)include", "$(MSBuildThisFileDirectory)src"] + [
        f"$(MSBuildThisFileDirectory){rel(p.dir, BY_NAME[r].dir)}\\include" for r in p.refs]
    # Copied decomp code (.c) gets a relaxed warning set until promoted to .cpp (plan §8.2).
    compile_items = "\n".join(
        f'    <ClCompile Include="{r}\\**\\*.cpp" />\n'
        f'    <ClCompile Include="{r}\\**\\*.c">\n'
        f'      <TreatWarningAsError>false</TreatWarningAsError>\n'
        f'      <WarningLevel>Level3</WarningLevel>\n'
        f'    </ClCompile>' for r in roots)
    header_items = "\n".join(
        ['    <ClInclude Include="include\\**\\*.hpp" />', '    <ClInclude Include="include\\**\\*.h" />'] +
        [f'    <ClInclude Include="{r}\\**\\*.hpp" />\n    <ClInclude Include="{r}\\**\\*.h" />' for r in roots])
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
        link = f"    <Link>\n      <SubSystem>{sub}</SubSystem>\n{deps}    </Link>\n"
    configs = ""
    for cfg in ("Debug", "Release"):
        dbg = "true" if cfg == "Debug" else "false"
        configs += f"""  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='{cfg}|x64'" Label="Configuration">
    <ConfigurationType>{ctype}</ConfigurationType>
    <UseDebugLibraries>{dbg}</UseDebugLibraries>
    <PlatformToolset>ClangCL</PlatformToolset>
  </PropertyGroup>
"""
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
    </ClCompile>
{link}  </ItemDefinitionGroup>
  <ItemGroup>
{compile_items}
  </ItemGroup>
  <ItemGroup>
{header_items}
  </ItemGroup>
  <ItemGroup>
{refs}
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <Import Project="{VCPKG_TARGETS}" Condition="Exists('{VCPKG_TARGETS}')" />
</Project>
"""


FOLDER_GUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
CPP_GUID = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
SOLUTION_ITEMS = ["Docs\\NATIVE_SOURCE_PORT_PLAN.md", "Docs\\ENEMY_DATA_SCHEME.md", "Docs\\TOOLS_INVENTORY.md",
                  "Docs\\ledgers\\SUMMARY.md", "Directory.Build.props", "vcpkg.json"]


def folder_guid(name: str) -> str:
    return "{" + str(uuid.uuid5(NS, "folder:" + name)).upper() + "}"


def solution() -> str:
    folders: list[str] = []
    for p in PROJECTS:
        if p.folder not in folders:
            folders.append(p.folder)
    lines = ["", "Microsoft Visual Studio Solution File, Format Version 12.00",
             "# Visual Studio Version 18", "VisualStudioVersion = 18.0.0.0",
             "MinimumVisualStudioVersion = 10.0.40219.1"]
    ordered = [BY_NAME["VS.Desktop"]] + [p for p in PROJECTS if p.name != "VS.Desktop"]  # default startup
    for p in ordered:
        path = rel(ROOT, p.dir / f"{p.name}.vcxproj")
        lines += [f'Project("{CPP_GUID}") = "{p.name}", "{path}", "{p.guid}"', "EndProject"]
    for f in folders:
        lines += [f'Project("{FOLDER_GUID}") = "{f}", "{f}", "{folder_guid(f)}"', "EndProject"]
    lines += [f'Project("{FOLDER_GUID}") = "Solution Items", "Solution Items", "{folder_guid("Solution Items")}"',
              "\tProjectSection(SolutionItems) = preProject"]
    lines += [f"\t\t{i} = {i}" for i in SOLUTION_ITEMS]
    lines += ["\tEndProjectSection", "EndProject"]
    lines += ["Global", "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
              "\t\tDebug|x64 = Debug|x64", "\t\tRelease|x64 = Release|x64", "\tEndGlobalSection",
              "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution"]
    for p in ordered:
        for c in ("Debug", "Release"):
            lines += [f"\t\t{p.guid}.{c}|x64.ActiveCfg = {c}|x64", f"\t\t{p.guid}.{c}|x64.Build.0 = {c}|x64"]
    lines += ["\tEndGlobalSection", "\tGlobalSection(SolutionProperties) = preSolution",
              "\t\tHideSolutionNode = FALSE", "\tEndGlobalSection",
              "\tGlobalSection(NestedProjects) = preSolution"]
    lines += [f"\t\t{p.guid} = {folder_guid(p.folder)}" for p in ordered]
    lines += ["\tEndGlobalSection", "EndGlobal", ""]
    return "\r\n".join(lines)


def ensure_dirs() -> None:
    for p in PROJECTS:
        for d in ["src", "include"] + p.extra_dirs:
            (p.dir / d).mkdir(parents=True, exist_ok=True)
    game = BY_NAME["VS.Game"]
    for m in GAME_MODULES:
        (game.dir / "src" / m).mkdir(parents=True, exist_ok=True)
        (game.dir / "include" / "vs" / "game" / m.lower()).mkdir(parents=True, exist_ok=True)


def main() -> int:
    ensure_dirs()
    for p in PROJECTS:
        (p.dir / f"{p.name}.vcxproj").write_text(vcxproj(p), encoding="utf-8", newline="\n")
    (ROOT / "VagrantStory.Native.sln").write_text(solution(), encoding="utf-8-sig", newline="")
    print(f"wrote VagrantStory.Native.sln and {len(PROJECTS)} projects")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
