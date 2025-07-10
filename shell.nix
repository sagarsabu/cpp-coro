{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  packages = with pkgs; [
    folly
    boost188
    gcc15
  ];
  inputsFrom = with pkgs; [
    folly
    boost188
  ];
}
