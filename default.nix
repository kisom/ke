{
  lib,
  stdenv,
  cmake,
  installShellFiles,
  ...
}:
let
  cmakeContent = builtins.readFile ./CMakeLists.txt;
  cmakeLines = lib.splitString "\n" cmakeContent;
  # Find the line containing set(KE_VERSION "...")
  versionLine = lib.findFirst (l: builtins.match ".*set\\(KE_VERSION \".+\"\\).*" l != null) (throw "KE_VERSION not found in CMakeLists.txt") cmakeLines;
  # Extract the version number
  version = builtins.head (builtins.match ".*set\\(KE_VERSION \"(.+)\"\\).*" versionLine);
in
stdenv.mkDerivation {
  pname = "ke";
  inherit version;

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    cmake
    installShellFiles
  ];

  cmakeFlags = [
    "-DENABLE_ASAN=on"
    "-DCMAKE_BUILD_TYPE=Debug"
  ];

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin
    cp ke $out/bin/

    installManPage ../ke.1

    runHook postInstall
  '';
}
