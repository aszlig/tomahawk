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
  } else {
    inherit system;
  };

  mkBuild = system: import ./. (genSystemAttrs system);

  supportedSystems = [
    "i686-linux" "x86_64-linux" "i686-mingw64" "x86_64-mingw64"
  ];

in {
  tomahawk = genAttrs supportedSystems mkBuild;
}
