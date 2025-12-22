set_project("holylua")
set_version("1.0.0")
set_languages("c11", "cxx17")

if is_plat("windows") then
    set_toolchains("mingw")
end

set_optimize("smallest")
add_cxflags("-O3", "-s")
add_cxflags("-ffunction-sections", "-fdata-sections")
add_ldflags("-Wl,--gc-sections")
add_cxflags("-Wall", "-Wextra", 
            "-Wno-unused-variable", 
            "-Wno-unused-but-set-variable",
            "-Wno-gnu-line-marker")

if is_plat("windows") then
    add_cxflags("-fPIC", "-pipe", "-fdiagnostics-color=always")
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
    
    after_install(function (target)
        local installdir = target:installdir()
        local libdir = path.join(installdir, "lib")
        local includedir = path.join(installdir, "include")
        
        if is_plat("windows") then
            local libfile = path.join(libdir, "holylua.lib")
            os.run("setx HOLY_LUA_LIB \"%s\"", libfile)
            os.run("setx HOLY_LUA_INCLUDE \"%s\"", includedir)
            print("Set HOLY_LUA_LIB=" .. libfile)
            print("Set HOLY_LUA_INCLUDE=" .. includedir)
        else
            local libfile = path.join(libdir, "libholylua.a")
            local bashrc = path.join(os.getenv("HOME"), ".bashrc")
            local lib_export = string.format("export HOLY_LUA_LIB=\"%s\"", libfile)
            local include_export = string.format("export HOLY_LUA_INCLUDE=\"%s\"", includedir)
            
            local content = io.readfile(bashrc) or ""
            if not content:find("HOLY_LUA_LIB", 1, true) then
                io.writefile(bashrc, content .. "\n" .. lib_export .. "\n" .. include_export .. "\n")
                print("Added HOLY_LUA_LIB and HOLY_LUA_INCLUDE to " .. bashrc)
            end
            
            os.setenv("HOLY_LUA_LIB", libfile)
            os.setenv("HOLY_LUA_INCLUDE", includedir)
        end
    end)

target("holylua")
    set_kind("binary")
    set_default(true)
    add_files("src/**.cpp")
    add_deps("holylua_static")
    add_linkdirs("lib")
    add_links("holylua")
    
    if is_plat("windows") then
        add_ldflags("-Wl,--gc-sections", "-static-libgcc", "-static-libstdc++", "-static")
        add_syslinks("user32")
    else
        add_ldflags("-Wl,--as-needed")
        add_syslinks("m")
    end
    
    after_install(function (target)
        local installdir = target:installdir()
        local bindir = path.join(installdir, "bin")
        
        if is_plat("windows") then
            local result = os.iorun("reg query HKCU\\Environment /v PATH")
            local current_path = ""
            
            if result then
                current_path = result:match("PATH%s+REG_[^%s]+%s+(.+)") or ""
            end

            if not current_path:find(bindir, 1, true) then
                local new_path = current_path ~= "" and (current_path .. ";" .. bindir) or bindir
                os.run("setx PATH \"%s\"", new_path)
                print("Added " .. bindir .. " to PATH")
            else
                print(bindir .. " already in PATH")
            end
        else
            local bashrc = path.join(os.getenv("HOME"), ".bashrc")
            local export_line = string.format("export PATH=\"%s:$PATH\"", bindir)
            
            local content = io.readfile(bashrc) or ""
            if not content:find(bindir, 1, true) then
                io.writefile(bashrc, content .. "\n" .. export_line .. "\n")
                print("Added " .. bindir .. " to PATH in " .. bashrc)
            end
            
            os.setenv("PATH", bindir .. ":" .. os.getenv("PATH"))
        end
    end)

target("holylua_static")
    add_installfiles("lib/holylua.*", {prefixdir = "lib"})
    add_installfiles("api/holylua_api.h", {prefixdir = "include"})
    
target("holylua")
    add_installfiles("bin/holylua*", {prefixdir = "bin"})
    add_installfiles("include/**", {prefixdir = "include"})