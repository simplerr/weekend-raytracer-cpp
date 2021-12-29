workspace "weekend-raytracer-cpp"
   configurations { "Debug", "Release" }
   language "C++"
   cppdialect "C++17"
   platforms "x64"
   characterset "ASCII"

   -- "Debug"
   filter "configurations:Debug"
      defines { "DEBUG", "_DEBUG" }
      flags { "MultiProcessorCompile", }
      symbols "On"
    
   -- "Release"
   filter "configurations:Release"
      defines { "NDEBUG" }
      flags { "MultiProcessorCompile" }
      symbols "On"
      optimize "Full"

project "weekend-raytracer-cpp"
   kind "ConsoleApp"
   targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
   objdir "%{wks.location}/bin/%{cfg.buildcfg}"
   location "%{wks.location}/bin/"

   -- Files
   files
   {
      "main.cpp",
   }

   -- "Debug"
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      debugformat "c7"

   -- "Release"
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

