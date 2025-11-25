{
  lib,
  stdenv,
  cmake,
  installShellFiles,
  ...
}:
stdenv.mkDerivation {
  pname = "ke";
  version = "1.3.5";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [ cmake installShellFiles ];

  cmakeFlags = [
    "-DENABLE_ASAN=on"
    "-DCMAKE_BUILD_TYPE=Debug"
  ];

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin
    cp ke $out/bin/

    runHook postInstall
  '';

  postInstall = ''
    installManPage ke.1
  '';
}
