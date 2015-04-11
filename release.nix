with import <nixpkgs/lib>;

let
  genSystemAttrs = system: if hasSuffix "-mingw64" system then {
    crossSystem = let
      is64 = hasPrefix "x86_64" system;
      realArch = if is64 then "x86_64" else "i686";
      realSystem = "${realArch}-w64-mingw32";
    in {
      config = realSystem;
      arch = if is64 then "x86_64" else "x86";
      libc = "msvcrt";
      platform = {};
      openssl.system = "mingw${optionalString is64 "64"}";
    };
  } else if system == "x86_64-apple-darwin" then {
    crossSystem = {
      config = "x86_64-apple-darwin13";
      arch = "x86_64";
      libc = "libSystem";
      platform = {};
      osxMinVersion = "10.7";
      openssl.system = "darwin64-x86_64-cc";
    };
  } else {
    inherit system;
  };

  mkBuild = system: import ./. (genSystemAttrs system);

  supportedSystems = [
    "i686-linux" "x86_64-linux" "i686-mingw64" "x86_64-mingw64"
    "x86_64-apple-darwin"
  ];

in {
  tomahawk = genAttrs supportedSystems mkBuild;
}