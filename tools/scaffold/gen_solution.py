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

import os
import uuid
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Native"
NS = uuid.UUID("6f1c2a52-3c1e-4c55-9a43-56d0a1f0b7e1")  # deterministic GUIDs
SOURCE_ROOTS = ("src", "include", "Legacy")
CODE = {".cpp": "ClCompile", ".c": "ClCompile", ".hpp": "ClInclude", ".h": "ClInclude"}


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
    Project("VS.Desktop", "Desktop", kind="exe", refs=["VS.Engine", "VS.Content", "VS.Game"], libs=["sdl3"]),
    Project("VS.Extract", "Tools\\Extract", kind="exe", console=True, refs=["VS.Engine", "VS.Content"],
            libs=["sdl3"]),
    Project("VS.Tests", "Tests", kind="exe", console=True, refs=["VS.Engine", "VS.Content", "VS.Game"],
            libs=["sdl3", "catch2"]),
]
BY_NAME = {p.name: p for p in PROJECTS}

# Import libraries per vcpkg package (vcpkg's lib\*.lib autolink does not reach the link line here).
VCPKG_LIBS = {
    "sdl3": {"Debug": ["SDL3.lib"], "Release": ["SDL3.lib"]},
    "catch2": {"Debug": ["Catch2d.lib"], "Release": ["Catch2.lib"]},
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
    return out


def vcxproj(p: Project, files: list[tuple[str, str]]) -> str:
    ctype = "Application" if p.kind == "exe" else "StaticLibrary"
    inc_dirs = ["$(MSBuildThisFileDirectory)include", "$(MSBuildThisFileDirectory)src"] + [
        f"$(MSBuildThisFileDirectory){rel(p.dir, BY_NAME[r].dir)}\\include" for r in p.refs + p.see]
    compile_items, header_items = [], []
    for kind, path in files:
        if kind == "ClInclude":
            header_items.append(f'    <ClInclude Include="{path}" />')
        elif path.endswith(".c"):
            # Copied decomp code (.c) gets a relaxed warning set until promoted to .cpp (plan §8.2).
            compile_items.append(f'    <ClCompile Include="{path}">\n'
                                 f'      <TreatWarningAsError>false</TreatWarningAsError>\n'
                                 f'      <WarningLevel>Level3</WarningLevel>\n'
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
    groups = ""
    for items in (compile_items, header_items):
        if items:
            groups += "  <ItemGroup>\n" + "\n".join(items) + "\n  </ItemGroup>\n"
    if refs:
        groups += f"  <ItemGroup>\n{refs}\n  </ItemGroup>\n"
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
SOLUTION_ITEMS = ["Docs\\NATIVE_SOURCE_PORT_PLAN.md", "Docs\\ENEMY_DATA_SCHEME.md", "Docs\\TOOLS_INVENTORY.md",
                  "Docs\\ledgers\\SUMMARY.md", "Directory.Build.props", "vcpkg.json"]


def solution() -> str:
    lines = ["", "Microsoft Visual Studio Solution File, Format Version 12.00",
             "# Visual Studio Version 18", "VisualStudioVersion = 18.0.0.0",
             "MinimumVisualStudioVersion = 10.0.40219.1"]
    ordered = [BY_NAME["VS.Desktop"]] + [p for p in PROJECTS if p.name != "VS.Desktop"]  # default startup
    for p in ordered:
        path = rel(ROOT, p.dir / f"{p.name}.vcxproj")
        lines += [f'Project("{CPP_GUID}") = "{p.name}", "{path}", "{p.guid}"', "EndProject"]
    lines += [f'Project("{FOLDER_GUID}") = "Solution Items", "Solution Items", "{guid_of("folder:Solution Items")}"',
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
              "\t\tHideSolutionNode = FALSE", "\tEndGlobalSection", "EndGlobal", ""]
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
    print(f"wrote VagrantStory.Native.sln and {len(PROJECTS)} projects")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
