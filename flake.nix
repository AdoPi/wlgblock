{
  description = "Gblocker - a Wayland gameboy locker";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            cage
            gcc
            libpng
            mesa
            libepoxy
            openal
            wayland
            wayland-protocols
            libxkbcommon
            scdoc
            gdb
          ];

          nativeBuildInputs = with pkgs; [
            pkg-config
          ];

          shellHook = ''
            echo "GBCC development environment loaded!"

	    meson setup --reconfigure build && ninja -C build
	    echo "Done. Run ./build/wlgblock  ./lock.gbc"
          '';

          # Environment variables for development
          PKG_CONFIG_PATH = "${pkgs.lib.makeSearchPath "lib/pkgconfig" (with pkgs; [ libpng mesa libepoxy openal wayland wayland-protocols libxkbcommon gtk4 SDL2 ])}";
          
          # Wayland environment
          # WAYLAND_DISPLAY = "wayland-0";
          
          # OpenGL
          LIBGL_DRIVERS_PATH = "${pkgs.mesa.drivers}/lib/dri";
        };

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "gbcc";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            scdoc
          ];

          buildInputs = with pkgs; [
            libpng
            mesa
            libepoxy
            openal
            wayland
            wayland-protocols
            libxkbcommon
            #gtk4
            #SDL2
          ];

          mesonFlags = [
#            "-Dprefix=${placeholder "out"}"
            "-Dgtk=enabled"
            "-Dman-pages=enabled"
          ];

          meta = with pkgs.lib; {
            description = "wlgblock";
            homepage = "https://github.com/AdoPi/wlgblock";
            license = licenses.gpl3;
            platforms = platforms.linux;
            maintainers = [ AdoPi ];
          };
        };
      }
    );
} 
