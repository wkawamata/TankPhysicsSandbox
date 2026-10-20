# Undo/Redo backlog

This is the single list of deferred Undo/Redo support for GUI operations in
Editor and Game. Keep implementation guidance in [undo-history.md](undo-history.md).

When deferring support, add one row in the same change. Identify the operation,
its code location, why it is deferred and the work needed to finish it. Include
any limits on what users can restore. Link an issue or PR when available.
Remove the row after implementation and verification; Git retains its history.

| Operation / screen | Code location | Reason and user impact | Remaining work | Issue / PR |
| --- | --- | --- | --- | --- |

No deferred items have been registered. This is not an audit of every existing GUI operation.

File writes/overwrite, file deletion, exports, simulation rewind and ordinary
viewport navigation are intentional exclusions under the current scope, rather
than deferred items. If a future task explicitly requires their Undo/Redo,
implement it or register the deferral here.
