# Playtest guide

**Things to try, and what to watch for.** No fighting-game vocabulary required —
where a term is needed it is explained here or in
[MECHANICS.md](MECHANICS.md).

```
build\debug\bin\mortal_wombat.exe
```

`F1` shows the collision boxes. `ESC` quits. Controls are in `AGENTS.md`.

**A rough answer is a complete answer.** "That felt bad" is genuinely useful.
The last three real bugs all came from a loose description of something looking
wrong, not from a precise report — that hit rate is better than my reasoning has
managed.

You can also answer "I don't know" to anything. That is information too: if a
question cannot be answered by playing, the game is not showing enough.

---

## Already answered

| Question | Answer | Result |
|---|---|---|
| Can you tell when a hit lands? | **Yes — the character lights up** | Hit feedback works. Lowers the priority of extra hit effects. |

---

## 1. Is attacking risky enough?

**Try this:** stand well away from the other fighter, out of reach. Press `V`
(high kick) so it swings at nothing.

**Watch for:** how long you are stuck afterwards, unable to move or attack.

- Does it feel like a real mistake, or does it barely cost anything?
- With a second player: if they swing and miss from close range, can you walk
  over and hit them before they recover?

**Why it matters:** the design says a big attack that misses should genuinely
hurt you. If it does not, every attack is free and there is no reason to be
careful. This is the single most important thing in the design document.

---

## 2. Is there any answer to jumping?

**Try this:** with a second player, have one person just jump at the other over
and over, pressing an attack button on the way down.

**Watch for:** can the person on the ground do *anything* about it? Can they hit
the jumper out of the air, or move out of the way, or block it and then hit
back?

- If the answer is "no, jumping always works" — that is a serious problem.
- If jumping seems useless and walking is always better, that is a problem too,
  in the other direction.

**Why it matters:** if jumping cannot be stopped, then jumping is always the
right move and walking around never happens. The whole ground game disappears.
Jump attacks only started working very recently, so nobody knows the answer yet.

*This is the question I was asking with "anti-air a jump-in" — sorry for the
jargon.*

---

## 3. Does jumping feel worth it?

**Try this:** walk across the stage. Then jump across it.

**Watch for:**

- Does jumping cover noticeably more ground than walking? *(It should — it is
  set to 1.8× walk speed, which is a guess.)*
- When you land, you are stuck for a moment. Does the extra distance feel worth
  that?
- When you press up, is there a delay before you leave the ground? Does it feel
  sluggish, or does it feel like a decision you committed to?
- Is the jump too floaty, or too heavy?

**Why it matters:** three numbers here are pure guesswork and cannot be checked
any other way.

---

## 4. Is blocking worth doing?

**Try this:** with a second player, hold block (`B`, or a shoulder button) while
they attack you.

**Watch for:**

- Do you take damage? *(You should not — blocking stops it entirely.)*
- After blocking, are you stuck for a moment before you can act? Does it feel
  fair, or does blocking feel like a trap?
- Can you tell you are blocking? The fighter goes pale with a light bar on the
  front edge.
- Is blocking better than just walking away?

---

## 5. Does the pacing feel right?

**Watch for:**

- The wait before a round starts is **1.5 seconds**. Too long? Too short?
- After a knockout, the pose holds for **2 seconds** before the next round.
- Fighters start about **a screen-width apart**. Does walking into range at the
  start of every round get boring?
- Does the camera feel steady, or does it shove around while you move?

---

## 6. Do the fighters look right?

They are currently narrow rectangles — 32 wide, 140 tall.

The design document asks for wombats that are *"round, heavy, and
short-limbed"*, which is close to the opposite shape.

**Watch for:** now that they move, jump and attack, what shape do they *want* to
be? Shorter and wider? How much?

**Why it matters:** body shape decides where the collision boxes go, so this
blocks a lot of later work. The height is from the design doc; the width was my
guess.

---

## Reporting

Three things, roughly:

1. **What you did** — "jumped in from about half a screen away"
2. **What happened** — "the other guy just walked backwards"
3. **What you expected** — even just "not that"

A report of *"if I move right, the other character moves left towards me"* found
a camera bug that had survived code review and every screenshot taken of it,
because it is invisible in a still image.

Describing the wrongness is enough. Diagnosing it is my job.
