{
  lib,
  installShellFiles,
  stdenv,
  ...
}:
stdenv.mkDerivation {
  pname = "ke";
  version = "1.3.4";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [ installShellFiles ];

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
