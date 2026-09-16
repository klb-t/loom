# Decision policy for the ChatADHD → Loom port

This is a process invariant for the port, not a choice of implementation technology.

## 1. Preserve optionality

Do not decide what does not need to be decided. Prefer a stable semantic contract plus replaceable implementations/configuration over a premature concrete choice.

A difference between historical sources is not automatically a conflict requiring one side to be deleted. First try to preserve both behind an abstraction, compatibility layer, feature policy, or versioned representation.

## 2. Rejection requires explicit acceptance

If two prior decisions or fundamental assumptions genuinely cannot coexist behind an abstraction, **do not silently choose one and discard the other**.

Record the conflict as OPEN and request explicit acceptance before rejecting either branch.

A quick conversational assent is not sufficient evidence of a durable architectural decision when a fundamental conflict is being resolved. The resolution must be explicit enough to identify what is being kept and what is being abandoned.

Exception: material that is clearly a joke, a throwaway experiment, an obvious mistake, or explicitly superseded by a better idea need not be preserved as a live architectural alternative.

## 3. Decision states

Use these states in design notes:

- `OBSERVED` — fact found in source/code/history.
- `PROPOSED` — suggestion, not accepted architecture.
- `OPEN` — unresolved choice/conflict.
- `COMPATIBILITY_BOUND` — must remain compatible for existing data/API/users, but may be wrapped by a richer future model.
- `ACCEPTED` — explicit stable decision.
- `SUPERSEDED` — explicitly replaced; record the replacement and reason.

Never infer `ACCEPTED` solely from implementation momentum.

## 4. Source priority

For porting behavior, distinguish:

1. actual behavior in the v0.07.10 source,
2. explicit user decisions,
3. the historical port prompt,
4. later architectural proposals.

When these disagree, record the disagreement. Do not rewrite history to make them appear consistent.

## 5. Compatibility is not ontology

The SQLite v4 schema and the historical C ABI are compatibility boundaries. Preserving them does not make them the final internal representation. Richer internals may sit behind adapters as long as existing databases and ABI behavior remain valid.
