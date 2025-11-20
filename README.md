# Astro

Astro is a small, readable blockchain node MVP.

What it can do today
- Create and validate a linear chain of blocks and signed transactions
- Verify prev-link, monotonic timestamps, merkle roots, and ECDSA signatures
- Optional proof‑of‑work (leading‑zero difficulty), configurable; genesis can skip or enforce
- Persist the chain to an append‑only log (./data/chain.log) and restore it
- Mine a block at a chosen difficulty
- Demos: TUI (restores and persists), store demo, miner demo

### Demo

![Image](https://github.com/user-attachments/assets/b5cc0f62-0b7b-4858-9b71-472d885673fb)

License: MIT