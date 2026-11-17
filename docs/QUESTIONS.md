**English** · [Русский](ru/QUESTIONS.md)

# Questions to the owner

A queue of questions the AI agent cannot answer by itself: architectural forks,
choices between options, anything that changes the plan. Questions are numbered Q1, Q2, … so as not
to confuse them with plan tasks V1, V2, …. Every question has options and a recommendation.
While a question is open, the agent works on other tasks instead of guessing.

The owner answers right in the file or in chat. The answer is moved to `DECISIONS.md`,
and the question to the "Closed" section.

## Open

There are no open questions.

## Closed

- **Q1. How the client gets the host's world** — decisions 16 and 17 in `DECISIONS.md` (2026-11-07).
- **Q2. Protocol draft** — accepted, decision 13 (2026-11-07).
- **Q3. What the agent may do on test rigs** — option B, decision 20 (2026-11-07).
- **Q4. A network thread or the main thread** — no threads, decision 23 (2026-11-07).
- **Q5. Vessel state beyond motion at stage 1** — plus fuel, decision 24 (2026-11-07).
- **Q6. Where the CI plugin build gets the Orbiter SDK** — build the core from the base commit, decision 25 (2026-11-07).
- **Q7. What the agent should do while there are no test rigs** — option A: session logic in the core (`HostLink`, `ClientLink`, `ClientClock`) was done in task S7 of stage 1 (2026-11-17).
