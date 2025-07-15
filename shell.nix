{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  packages = with pkgs; [
    boost188
    gcc15
  ];
  inputsFrom = with pkgs; [
    boost188
  ];
}
