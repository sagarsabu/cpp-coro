{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  packages = with pkgs; [
    folly
    boost
  ];
  inputsFrom = with pkgs; [
    folly
    boost
  ];
}
