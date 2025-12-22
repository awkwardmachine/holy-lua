# Holy Lua

<!-- markdownlint-disable-next-line MD033 -->
<img src="https://media.discordapp.net/attachments/1432356922417348699/1448079529410039950/logo1.png?ex=693beee7&is=693a9d67&hm=47f7dbf95516fb557d63b3d3402858d0b8d5b7a2037bbc12917603d23695afc3&=&format=webp&quality=lossless" alt="Holy Lua Logo" width="200">

Lua with a type system.

Holy Lua is a statically typed take on Lua that compiles to C. Same elegant syntax, fewer runtime surprises.

## Why This Exists

I'm Nil, and I created Holy Lua out of love for Lua and ambition for something bigger. I wanted to build an operating system, but I also wanted to keep my code simple and readable. Most systems languages force you to choose between simplicity and control. I didn't want to choose.

Holy Lua was born from that tension. While the current implementation can't quite build an OS yet, it's close. The foundation is there: direct C compilation, predictable memory layout, zero-cost abstractions. The missing pieces are coming.

This is Lua that grew up without losing its soul.

## The Problem

Lua is minimal and beautiful. It gets out of your way. But in bigger projects, that freedom can bite you. `nil` appears unexpectedly, types shift mid-function, things break at runtime.

Holy Lua keeps what makes Lua great and adds the type safety you wish you had.
```lua
local x: number? = nil

if x != nil then
    print(x)
else
    print("X is nil")
end
```

You're writing down what you already know. No extra ceremony.

## Two Modes

**Scripting Mode** for exploration and quick work

No `main()` required. Code runs top-to-bottom:
```lua
print("Hello world!")
local score: number = 100
print("Your score is", score)
```

Great for scripts, configs, or REPL-style coding.

**Systems Mode** for building real things

Add a `main()` function:
```lua
function main()
    local app = initialize_system()
    app.run()
    cleanup(app)
end
```

Same language, different context.

## Data Structures

Holy Lua provides proper data structures: structs, enums, and classes.

**Structs:**
```lua
struct Player
    name: string
    health: number
    position: Point
end

local hero = Player { "Arin", 100, Point { 10, 20 } }
print(hero.name)
```

**Enums:**
```lua
enum GameState
    Menu
    Playing
    Paused
    GameOver
end

local state: GameState = GameState.Playing
```

**Classes:**
```lua
class Enemy
    public health: number
    public damage: number

    function __init(health: number, damage: number)
        self.health = health
        self.damage = damage
    end

    function take_damage(amount: number)
        self.health -= amount
    end
end
```

## The Safety Net

No more "attempt to index nil" at 3 AM. No more accidental string + number operations. No more guessing what a function returns.

The compiler catches issues before you run anything. Fail at compile time, not in production.

## High Level Simplicity, Low Level Control

Holy Lua gives you high-level abstractions when you want them and low-level control when you need it. Write in a clean, expressive syntax that compiles directly to C. No runtime overhead, no garbage collection surprises, just predictable performance.

When you need to drop down to C, the interop is seamless. When you don't, the language stays out of your way.

## The Tooling

Holy Lua ships with a language server that makes development smooth:

**Before you even compile:**

The LSP catches errors as you type. See type mismatches, undefined variables, and incorrect function calls highlighted immediately. Most errors are caught before you save the file.

**Inline information:**

Inlay hints show types without cluttering your code. Hover over any variable to see its full type signature. Function calls display parameter names and expected types as you write them.

**Built-in documentation (coming soon):**

HolyLuaDoc integrates directly into the LSP. Document your code inline:
```lua
-- (this would be inside a class as a constructor)
---@param health number The starting health value
---@param damage number The base damage amount
---@return Enemy A new enemy instance
function new(health: number, damage: number): Enemy
    return Enemy(health, damage)
end
```

Hover over any function or type to see its documentation. No need to jump to definitions or search through files.

**Quick Fixes:**

Get suggestions for fixes when something's wrong. See function signatures inline as you call them. Receive corrections for typos with "did you mean..." suggestions. Navigate your codebase with go-to-definition and find-all-references.

The tooling gets out of your way until you need it, then it's there immediately.

## Who It's For

Holy Lua is for developers who love Lua's elegance but need reliability. For those who want to prototype quickly but deploy safely. For programmers who need low-level control without the ceremony.

Holy Lua is familiar, but new, and that is the magic of it.

## Getting Started

See [BUILD.md](BUILD.md) for build instructions.