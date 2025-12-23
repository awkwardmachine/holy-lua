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

add_cxflags(
    "-Wall", "-Wextra",
    "-Wno-unused-variable",
    "-Wno-unused-but-set-variable",
    "-Wno-gnu-line-marker",
    "-fPIC", "-pipe", "-fdiagnostics-color=always"
)

set_targetdir("bin")
set_objectdir("obj")
add_includedirs("include")

target("holylua_static")
    set_kind("static")
    set_targetdir("lib")
    set_basename("holylua")
    add_files("api/holylua_api.c")

    if is_plat("windows") then
        set_prefixname("")
        set_suffixname("")
        add_syslinks("user32")
    else
        set_prefixname("lib")
    end

    after_install(function(target)
        local installdir = target:installdir()
        print("holylua_static installed to " .. installdir)
        io.stdout:flush()
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

    after_install(function(target)
        local installdir = target:installdir()
        local bindir = path.join(installdir, "bin")
        local libdir = path.join(installdir, "lib")
        local includedir = path.join(installdir, "include")

        if is_plat("windows") then
            local libfile = path.join(libdir, "holylua.lib")
            os.run("setx HOLY_LUA_LIB \"%s\"", libfile)
            os.run("setx HOLY_LUA_INCLUDE \"%s\"", includedir)
            print("Set HOLY_LUA_LIB=" .. libfile)
            print("Set HOLY_LUA_INCLUDE=" .. includedir)
        else
            local is_system_install = installdir:startswith("/usr") or installdir:startswith("/opt")
            
            if is_system_install then
                local profiledir = "/etc/profile.d"
                local profilefile = path.join(profiledir, "holylua.sh")
                
                local content = string.format([[# holylua environment variables
export PATH="%s:$PATH"
export HOLY_LUA_LIB="%s"
export HOLY_LUA_INCLUDE="%s"
]], bindir, path.join(libdir, "libholylua.a"), includedir)

                os.mkdir(profiledir)
                io.writefile(profilefile, content)
                os.run("chmod 644 " .. profilefile)
                
                print("")
                print("holylua installed globally to " .. installdir)
                print("Environment variables set in " .. profilefile)
                print("")
                print("To use holylua immediately, run: source " .. profilefile)
                print("Or open a new terminal session")
            else
                local home = os.getenv("HOME")
                local bashrc = path.join(home, ".bashrc")
                local bashrc_content = io.readfile(bashrc) or ""

                if not bashrc_content:find("# === holylua ===", 1, true) then
                    local holylua_section = string.format([[

# === holylua ===
export PATH="%s:$PATH"
export HOLY_LUA_LIB="%s"
export HOLY_LUA_INCLUDE="%s"
# === /holylua ===
]], bindir, path.join(libdir, "libholylua.a"), includedir)
                    
                    io.writefile(bashrc, bashrc_content .. holylua_section)
                    print("Added holylua configuration to ~/.bashrc")
                else
                    print("holylua configuration already exists in ~/.bashrc")
                end

                print("")
                print("holylua installed to " .. installdir)
                print("")
                print("To use holylua immediately, run: source ~/.bashrc")
                print("Or open a new terminal session")
            end
            
            io.stdout:flush()
        end
    end)

target("holylua_static")
    add_installfiles("lib/(holylua.*)", { prefixdir = "lib" })
    add_installfiles("api/(holylua_api.h)", { prefixdir = "include" })

target("holylua")
    add_installfiles("bin/(holylua*)", { prefixdir = "bin" })
    add_installfiles("include/(**.h)", { prefixdir = "include" })
    add_installfiles("include/(**.hpp)", { prefixdir = "include" })