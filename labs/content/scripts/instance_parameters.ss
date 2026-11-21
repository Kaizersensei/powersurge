// Authored defaults and explicit runtime overrides. GPL-3.0-or-later. Signed: Atlas.
object "BSL Instance Parameters"
{
    authored = {};
    overrides = {};
    state "main" {}
    fun value(key, fallback) { return overrides.has(key) ? overrides[key] : (authored.has(key) ? authored[key] : fallback); }
    fun author(key, value) { authored[key] = value; }
    fun override(key, value) { overrides[key] = value; }
    fun reset() { overrides.clear(); }
}
