-- include subprojects
includes("lib/commonlibsse-ng")

-- set project constants
set_project("WhispersOfApocrypha")
set_version("1.1.3")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")

-- define targets
target("WhispersOfApocrypha")
    add_rules("commonlibsse-ng.plugin", {
        name = "WhispersOfApocrypha",
        author = "W1ter",
        description = "Lovecraftian madness in the ink of Apocrypha: whispers, a green nightmare and the eyes of Hermaeus Mora"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
