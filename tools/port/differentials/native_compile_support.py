"""Compiler settings shared by native C++ differential cases."""

COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
COMPILER_OPTIONS = (
    "/J",
    "/clang:-Wno-error=int-conversion",
    "/clang:-Wno-error=incompatible-pointer-types",
    "/clang:-Wno-error=incompatible-function-pointer-types",
    "/clang:-Wno-error=implicit-function-declaration",
    "/clang:-Wno-error=implicit-int",
    "/clang:-Wno-error=return-type",
    "/clang:-Wno-error=int-to-pointer-cast",
    "/clang:-Wno-c2x-extensions",
    "/clang:-Wno-unused",
    "/clang:-Wno-parentheses",
    "/clang:-Wno-missing-braces",
    "/clang:-Wno-sign-compare",
    "/clang:-Wno-pointer-sign",
    "/clang:-Wno-switch",
    "/clang:-Wno-unknown-pragmas",
    "/clang:-Wno-microsoft-anon-tag",
    "/clang:-fms-extensions",
    "/clang:-fno-finite-loops",
    "/clang:-fno-strict-return",
    "/clang:-fno-delete-null-pointer-checks",
    "/clang:-fwrapv",
    "/clang:-Wno-register",
    "/clang:-Wno-writable-strings",
)
