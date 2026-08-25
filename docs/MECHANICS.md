# Mechanics, in plain language

**What every term means, why it matters, and where it lives in this project.**

This exists so that discussions about the game do not require fighting-game
vocabulary as a prerequisite. `DESIGN.md` says what the game *is*; this says
what the words in it *mean*.

Nothing here is a decision. Where a number appears it is quoting `DESIGN.md`
§4.4 or §4.5's frame values, and where something is unsettled it says so.

---

## The whole game in one idea

A fighting game is a series of **guesses about time and distance**.

Every attack takes time to come out. During that time you cannot do anything
else. If you guess right — you attacked when they were close and busy — you hit
them. If you guess wrong, you are stuck doing nothing while they hit you.

That is the entire game. Everything below is vocabulary for parts of it.

**This game asks one version of that question:** *"was that worth committing
to?"* You picked a button, at a distance, at a moment, and now you live with it.
`DESIGN.md` §3 is where that is decided and why; it is repeated here only
because most of the words below are easier to understand once you know what
they are all in service of.

---

## An attack, frame by frame

The game runs at 60 frames per second, and every duration is counted in frames.
One frame is 1/60th of a second. Sixty frames is one second.

An attack has three phases, always in this order:

```
     STARTUP              ACTIVE            RECOVERY
  ┌──────────────┐   ┌──────────────┐  ┌──────────────────┐
  │ winding up   │   │ can hit      │  │ putting the limb │
  │ cannot hit   │   │ this is the  │  │ back, cannot hit │
  │ can be hit   │   │ only window  │  │ can be hit       │
  └──────────────┘   └──────────────┘  └──────────────────┘
```

- **Startup** — how long before the attack can hurt anyone. Low startup means
  fast. High punch is 7 frames, so about an eighth of a second.
- **Active** — the only frames where the attack can actually connect. Usually
  short: high punch is 3 frames.
- **Recovery** — how long you are stuck afterwards. **This is the price.** High
  punch is 16 frames, high kick is 20 — a third of a second where you can do
  nothing at all.

**The trade is always speed versus price.** Light punch is fast and cheap
(4 startup, 8 recovery). High kick is slow and expensive (9 startup, 20
recovery) but does three times the damage.

> In this project: `data/characters/george.toml`, and the table in
> `DESIGN.md` §4.5. Press `F1` in game to see the active window drawn in red.

---

## Hitboxes and hurtboxes

Two invisible rectangles, and the whole of collision:

- **Hurtbox** — the area where a fighter *can be hit*. Roughly their body.
- **Hitbox** — the area an attack *reaches into*, only during its active frames.

**An attack connects when a hitbox overlaps a hurtbox.** That is all. There is
no other rule.

`F1` draws them: blue is a hurtbox, red is a hitbox, yellow is the pushbox
(the body, which stops two fighters standing in the same place).

> This is why the jump attack was broken: its hitbox was positioned at head
> height for a *standing* fighter. In the air, the fighter rose and took the
> hitbox with them, so it passed above the opponent every time.

---

## Kinds of move

"How many moves does a character have?" is ambiguous until you say which of
these you are counting. This is the vocabulary the genre uses.

### Normals

**Attacks done with a plain button press.** No motion, no meter. The bread and
butter of the game — most of what you throw in a match is a normal.

Every normal usually has three variants depending on what the fighter is doing,
and each behaves differently:

| Variant | Input | Typical role |
|---|---|---|
| Standing | button | general purpose |
| Crouching | down + button | low attacks, ducking under things |
| Jumping | button while airborne | approach, hitting from above |

**This is where counting gets confusing.** With four buttons, "four normals"
and "twelve normals" describe the same character — four buttons × three stances.
Both are correct; they are counting different things.

### Command normals

**A direction plus a button that produces a genuinely different move**, not just
the crouching or jumping version of a normal.

Say forward + heavy punch is a distinct overhead attack that must be blocked
standing. That is a command normal. It costs no meter and needs no motion, but
it is its own move with its own frame data.

Characters usually have none to a handful.

### Specials

**Motion input plus a button.** A fireball, an uppercut, a charge attack. These
are what make a character *that character* — most of a fighter's identity lives
in two to four specials.

They cost nothing but are harder to execute, and they are usually stronger or
stranger than any normal.

### Supers

**Bigger specials that cost meter.** Depending on the game they are called
supers, ultras, criticals, or Fatal Blows. Same idea: spend a resource, get an
outsized attack.

A game with no meter has no supers. That is a legitimate choice, not an omission.

### Throws

**A close-range grab that beats blocking entirely.** Their whole job is to
answer a defender who is simply holding block — without them, blocking has no
downside.

Most games make them escapable with a matching input, so they are a guess rather
than a guarantee.

### Universal mechanics

Not moves, but things every character can do: dashing, parrying, rolling.
Everyone has them; how much they are worth varies by character.

This game'''s universal mechanics are the four below — meter, a commitment
release, Just Defend, and a combo cap.

---

## What the "twelve or ten moves" question was

`DESIGN.md` §4.5 said *"Twelve moves total"* above a table that listed:

- four standing normals (light and heavy punch, light and heavy kick)
- crouching versions of those four
- one jump attack
- one special

That is **ten**, counting each stance variant separately. Two are missing, and
nothing in the document says which.

The likeliest explanation is that the table intended four *jumping* normals —
one per button — which gives four standing plus four crouching plus four
jumping, exactly twelve, with the special counted separately as a thirteenth.
But that is a guess, and this project does not guess at design.

**The question is settled, by being dissolved rather than answered.** Dropping
the block button freed an input — the old design spent five buttons on four
attacks plus block, and blocking is now a direction. §4.1 spends that freed
input on a third strength, giving **six attack buttons**: light, medium and
heavy, in punch and kick.

So the count now falls out of the button set instead of being asserted in prose:
six standing normals, six crouching, a jump attack and a special. §4.5's old
table is superseded, and the code has not caught up yet — `ROADMAP.md` tracks
that.

The passage below is kept because the *reasoning* still applies to any future
change of this kind. Four attack buttons with hold-back
blocking is exactly the King of Fighters layout, which is the family this
project has now chosen.

## Four things this game has decided to have

The design has settled on four systems. **What each one *is* lives in
`DESIGN.md` §4.6; every number in them is still open.** Here is what the names
mean.

### Meter

A bar that fills as the match goes on, which you spend to do things you
otherwise could not.

The interesting part is not the bar, it is the **choosing**. If a meter does one
thing, spending it is not a decision — you spend it when you can. If it does
several, every spend is a small sacrifice of the others. That is where the depth
comes from, and it is why this game gives it **three** jobs rather than five:
the gap between the best use and the second-best is what has to stay honest, and
one person has to be able to check it by hand.

### A commitment release

A way to buy your way out of an attack you have already started — you swung,
you can see it is going to miss, and you spend meter to cut the recovery short
and not be punished for it.

Guilty Gear's Roman Cancel is the famous one and it is widely loved, because it
is general-purpose: it is not a move with a use, it is a tool players find their
own uses for.

**This game has exactly one, and it is expensive.** One, because two mechanics
doing the same job just split the importance between them. Expensive, because a
cheap way out of a bad commitment is a direct attack on the question the game is
asking.

### Just Defend (also called instant block)

Blocking, but at exactly the right moment — a much tighter window than normal
blocking, with a reward for hitting it.

It is often confused with a **parry**, and the difference is what happens when
you get it wrong. Miss a Just Defend and you have simply *blocked*. Miss a parry
and you are *hit*. This game chose Just Defend for that reason: it rewards
discipline without punishing you twice, and it behaves far better online, where
you may be reacting to a moment the network later revises.

### A combo cap

A hard limit on how much damage one opening can produce.

**Combo:** a sequence of attacks where the later ones are unavoidable because
the earlier ones left you unable to move. Getting hit once and then watching
helplessly is normal in fighting games; the question is how long it lasts.

Many games use *scaling* — later hits do less — which discourages long combos
without actually stopping them. This game uses a hard cap instead. A game about
whether a commitment was worth it cannot answer *"yes, it won the entire
round"*.

---

## What happens when you get hit

**Hitstun.** For a set number of frames after being hit, the defender cannot do
anything. They are frozen. High punch gives 18 frames — roughly a third of a
second where they cannot block, move, or attack.

**This is the reward for hitting someone.** Damage is almost secondary: the real
prize is that they cannot act while you can.

**Blockstun** is the same thing, but shorter, and it happens when they blocked.
High punch gives 12 frames instead of 18.

> **Why blockstun is always shorter:** blocking has to be better than being hit,
> or nobody would block. There is a test asserting this holds for every move.

---

## Frame advantage — who moves first

This is the one piece of arithmetic that runs the whole game.

Say you throw a high punch and they **block** it:

- They are frozen for **12 frames** (blockstun)
- You are stuck for **16 frames** (recovery)

You are stuck 4 frames longer than they are. **They get to act first.** If they
have a 4-frame attack, they hit you before you can do anything about it.

Now say the punch **hits** instead:

- They are frozen for **18 frames** (hitstun)
- You are stuck for **16 frames** (recovery)

Now *you* are free 2 frames before they are. You get to act first.

**That is why hitting is good and getting blocked is bad**, in a way that has
nothing to do with the damage number. Hitting buys you time. Getting blocked
costs you time.

> Nothing in this project measures or displays this yet. A "frame data readout"
> is one of the three things `DESIGN.md` §6 wants in training mode.

---

## The words I used that I should have explained

### Whiff

Attacking and **missing entirely** — nothing was there. You are now stuck in
recovery for nothing, and they can walk up and hit you for free.

`DESIGN.md` §3 asks that whiffing a heavy attack be *genuinely punishing*. That
is the single most important feel target in the design.

### Punish

Hitting someone **because they are stuck** — during their recovery, when they
cannot defend. Not a special move; just an attack thrown at the right moment.

"Can you punish a whiffed heavy?" means: they swung and missed, are they stuck
long enough that you can walk over and hit them before they recover?

### Jump-in

Jumping toward someone and attacking on the way down. It is a strong approach:
you cover distance *and* attack at the same time, and you come from an angle
that ground attacks may not reach.

### Anti-air

**Hitting someone out of the air before they land on you.** Usually by attacking
upward as they descend.

**Why it matters, and it is not a niche concern:** if jumping in cannot be
stopped, then jumping is always the right move. Walking becomes pointless,
spacing becomes pointless, and the ground game — which is most of what
`DESIGN.md` §3 describes as the intended feel — stops existing. The game becomes
two people jumping at each other.

So "can a grounded defender anti-air a jump-in?" really means: **is there any
answer to jumping, or does jumping just win?**

That is a foundational question, not an advanced one. It is also completely
unknown here, because jump attacks only started working recently.

### Neutral

The part of the match where nobody is committed to anything — both fighters
moving, looking for an opening.

**In this game neutral is a distinct place you stand, not a thing you drift in
and out of.** Attacking is how you leave it, and leaving it wrongly is meant to
cost you. Some modern fighting games deliberately blur that line so that a
missed poke and a full attack are nearly the same action; this one deliberately
does not (`DESIGN.md` §3).

### Spacing

Standing at a distance where *your* attacks reach and *theirs* do not. The main
skill in a game like this, and the reason attack reach and walk speed matter so
much.

### Trade

Both fighters hit each other on the same frame. Both take damage.

> This project resolves trades symmetrically on purpose: both attacks are
> checked before either takes effect, so neither player wins by being processed
> first.

### Mix-up

Making an attack that must be defended two different ways, so the defender has
to guess. Whether this game has them is undecided — see
`drawing-board/RULESET.md`, which is where mechanics still being decided live.

---

## The rock-paper-scissors underneath

A fighting game works when the options beat each other in a loop, so no single
option is always right:

```
   Attack  ──beats──▶  Approach (they walked into it)
      ▲                      │
      │                      │ beats
   beaten by                 ▼
      │                   Waiting
   Block/Wait  ◀──beats──  Attack (they whiffed, you punish)
```

**When one option has no answer, the loop breaks and the game gets boring.** The
questions worth asking during a playtest are all versions of "does something
here have no answer?"

Jumping is the most common thing to break the loop, which is why anti-air keeps
coming up.

---

## What exists right now

This document explains what the words mean. **It deliberately does not track
what is built or what is planned** — `ROADMAP.md` owns that, and a second copy
would drift.

## Deliberately simple

`DESIGN.md` §3 asks for a slow, readable, commitment-based game, and §10 breaks
any remaining tie toward the simpler option.

That is not a limitation to work around. A game where every attack is a single
decision with a visible cost is *easier to read, easier to learn, and easier to
tune* — and it is exactly what "two people can pick it up in ninety seconds"
requires.

Which mechanics the game ends up with is being decided in
`drawing-board/RULESET.md`, under the rule in ADR 0019: one mechanic per role,
adopted for what it does here rather than for where it came from.
