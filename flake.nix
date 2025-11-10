{
  description = "flake to build cpp-coro";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  };

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    devShells.${system}.default = pkgs.mkShell {
      # These packages are used by your build (CMake, compiler, etc.)
      nativeBuildInputs = with pkgs; [
        cmake
        gcc15
        pkg-config
      ];

      # These are the libraries your code links against
      buildInputs = with pkgs; [
        openssl
        boost188
      ];

      # Ensure CMake can find OpenSSL and Boost easily
      shellHook = ''
        export CMAKE_PREFIX_PATH=${pkgs.openssl}:${pkgs.boost188}:$CMAKE_PREFIX_PATH
      '';
    };
  };
}
