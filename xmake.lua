set_project("holylua")
set_version("1.0.0")

set_languages("c11", "cxx17")

set_optimize("smallest")
add_cxflags("-Os", "-s", "-flto")

add_cxflags("-ffunction-sections", "-fdata-sections")
add_ldflags("-Wl,--gc-sections")

add_cxflags("-Wall", "-Wextra", 
            "-Wno-unused-variable", 
            "-Wno-unused-but-set-variable",
            "-Wno-gnu-line-marker")

if is_plat("windows") then
    add_cxflags("/EHsc", "/Zc:__cplusplus", "/Os")
else
    add_cxflags("-fPIC", "-pipe", "-fdiagnostics-color=always")
end

set_targetdir("bin")
set_objectdir("obj")
add_includedirs("include")

target("holylua_static")
    set_kind("static")
    set_targetdir("lib")
    add_files("api/holylua_api.c")
    set_basename("holylua")
    if is_plat("windows") then
        set_prefixname("")
        set_suffixname("")
        add_syslinks("user32")
    else
        set_prefixname("lib")
    end

target("holylua")
    set_kind("binary")
    set_default(true)
    add_files("src/**.cpp")
    add_deps("holylua_static")
    add_linkdirs("lib")
    add_links("holylua")
    
    if is_plat("windows") then
        add_ldflags("/DEBUG:NONE", "/LTCG", "-static-libgcc", "-static-libstdc++")
        add_syslinks("user32")
    else
        add_ldflags("-Wl,--as-needed")
        add_syslinks("m")
    end