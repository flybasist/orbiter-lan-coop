**English** · [Русский](ru/QUESTIONS.md)

# Questions to the owner

A queue of questions the AI agent cannot answer by itself: architectural forks,
choices between options, anything that changes the plan. Questions are numbered Q1, Q2, … so as not
to confuse them with plan tasks V1, V2, …. Every question has options and a recommendation.
While a question is open, the agent works on other tasks instead of guessing.

The owner answers right in the file or in chat. The answer is moved to `DECISIONS.md`,
and the question to the "Closed" section.

## Open

- **Q13. How to transfer vessel mechanisms (stage 4A)** — research 3.21, `ORBITER-INTERNALS.md`, section 21. Options:
  - **A (recommended).** A combination: general animation transfer (any class, as a TCP event) plus DeltaGlider Lua commands so that the gear and touchdown points change for real; NAV frequencies as a separate message.
  - **B.** General animation transfer only: simple and for any class, but the internal vessel state (gear touchdown points) does not change.
  - **C.** DeltaGlider Lua commands only: real mechanisms, but one vessel class works.
- **Q12. How to review a session flight (stage 5, item 6)** — research 3.19, `ORBITER-INTERNALS.md`, section 20. The stock recording cannot be started from a plugin; options:
  - **A (recommended).** The host writes Orbiter recorder files itself (`.pos`, `.att`, `system.dat`, a `Playback` scenario): review is the stock playback, the format is text and simple. Cons: vessels created during the session do not get into playback; the format is undocumented.
  - **B.** An OLC log and viewer of our own: independent of the Orbiter format, but a separate program or window.
  - **C.** Record with the stock Ctrl+C key on the flight director's client with graphics: no code, but usability is not verified and a graphical client is needed.

## Closed

- **Q1. How the client gets the host's world** — decisions 16 and 17 in `DECISIONS.md` (2026-11-07).
- **Q2. Protocol draft** — accepted, decision 13 (2026-11-07).
- **Q3. What the agent may do on test rigs** — option B, decision 20 (2026-11-07).
- **Q4. A network thread or the main thread** — no threads, decision 23 (2026-11-07).
- **Q5. Vessel state beyond motion at stage 1** — plus fuel, decision 24 (2026-11-07).
- **Q6. Where the CI plugin build gets the Orbiter SDK** — build the core from the base commit, decision 25 (2026-11-07).
- **Q7. What the agent should do while there are no test rigs** — option A: session logic in the core (`HostLink`, `ClientLink`, `ClientClock`) was done in task S7 of stage 1 (2026-11-17).
- **Q8. The plan of stages 4 and 5** — the default options: stage 5 first, research 4.1 and 5.6 at once; the flight director — the server setting `Director`; one-time presses — the pilot only; the plan is approved; decision 46 (2026-11-18).
- **Q9. What to do after 3.17 before the tests by people** — only research 4.1 and 5.6, the code of stages 4 and 5 — after the tests; decision 47 (2026-11-18).
- **Q10. What textures and kernels go into the test archive** — option B: the SPICE kernels in the archive, Earth textures — by a link to Orbiter 2016 in the instructions; decision 50 (2026-11-18).
- **Q11. What the navigator changes in the pilot's vessel** — postponed together with the crew (part 4B); what to send to everyone is decided in 4A; decision 52 (2026-11-18).
