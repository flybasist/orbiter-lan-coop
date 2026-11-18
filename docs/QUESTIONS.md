**English** · [Русский](ru/QUESTIONS.md)

# Questions to the owner

A queue of questions the AI agent cannot answer by itself: architectural forks,
choices between options, anything that changes the plan. Questions are numbered Q1, Q2, … so as not
to confuse them with plan tasks V1, V2, …. Every question has options and a recommendation.
While a question is open, the agent works on other tasks instead of guessing.

The owner answers right in the file or in chat. The answer is moved to `DECISIONS.md`,
and the question to the "Closed" section.

## Open

### Q8. The plan of stages 4 and 5 (2026-11-18)

The detailed plan is in [PLAN.md](PLAN.md), section "Stages 4 and 5: detailed plan". Four answers are needed.

1. **Order of the stages.** A — stage 5 (flight director) first, then 4 (crew); B — stage 4 first;
   C — stage 5, and research 4.1 and 5.6 right away, in parallel. **Recommendation: C.** The shared
   pause, saves and watching are needed already in lessons and checks by people, and stage 5 is
   almost all plugin code; stage 4 needs research of cockpit clicks, which is worth starting early.
   If the space club needs the crew more — B.
2. **How the host recognises the flight director.** A — a server setting `Director = <player name>`;
   B — the FD is whoever connected from the same machine (address 127.0.0.1); C — all clients may ask
   for pause and warp. **Recommendation: A** (empty by default, no FD): explicit, works with the FD on
   a separate machine too; B breaks if the FD does not sit at the server; C gives chaos in a lesson.
3. **The navigator's single key presses** (autopilots, RCS mode). A — the pilot only; B — both.
   **Recommendation: A to start with:** then the navigator surely cannot take over control; B can be
   added later if the club asks.
4. **Approve the plan** (tasks 4.1–4.6 and 5.1–5.8, criteria) or correct it.

## Closed

- **Q1. How the client gets the host's world** — decisions 16 and 17 in `DECISIONS.md` (2026-11-07).
- **Q2. Protocol draft** — accepted, decision 13 (2026-11-07).
- **Q3. What the agent may do on test rigs** — option B, decision 20 (2026-11-07).
- **Q4. A network thread or the main thread** — no threads, decision 23 (2026-11-07).
- **Q5. Vessel state beyond motion at stage 1** — plus fuel, decision 24 (2026-11-07).
- **Q6. Where the CI plugin build gets the Orbiter SDK** — build the core from the base commit, decision 25 (2026-11-07).
- **Q7. What the agent should do while there are no test rigs** — option A: session logic in the core (`HostLink`, `ClientLink`, `ClientClock`) was done in task S7 of stage 1 (2026-11-17).
