// Generic registry-backed character body. No character/model mapping lives in this script. Signed: Mech.
using SurgeEngine.Actor;
using SurgeEngine.Level;
using SurgeEngine.Vector2;
object "BSL Registry Character" is "entity", "awake", "private", "gizmo", "editor-gizmo"
{
    actor = null;
    controller = spawn("BORController");
    character = null;
    state "main" {}
    fun initialize(definition)
    {
        if(character != null) return character == definition;
        model = controller.characterModel(definition);
        if(model == null) return false;
        if(actor == null) actor = Actor("BOR " + model);
        if(!controller.initCharacter(definition)) {
            actor.visible = false;
            return false;
        }
        character = definition;
        actor.visible = true;
        return true;
    }
    // Other actor wrappers may delegate to the same native component. Main menu ownership is unchanged.
    fun beginConversation(initiator, definition, range, calendarFactor) { return controller.beginConversation(initiator, definition, range, calendarFactor); }
    fun chooseConversation(choice) { return controller.chooseConversation(choice); }
    fun setInteraction(definition) { return controller.setInteraction(definition); }
    fun resetInteractionBinding() { return controller.resetInteractionBinding(); }
    fun interactionDefinition() { return controller.interactionDefinition(); }
    fun onLeaveEditor() { if(character != null) controller.rebase(); }
    fun onRenderGizmos(x, y) { if(character != null) controller.onRenderGizmos(x, y); }
}

// Trusted event/setup adapter. Owns no character state and caches no live body handles. Signed: Mech.
object "BSL Character Registry"
{
    registry = spawn("BORController");
    state "main" {}
    fun definitionAt(index) { return registry.characterAt(index); }
    fun selectedDefinition() { return registry.selectedCharacter(); }
    fun find(definition) { return registry.characterEntity(definition); }
    fun select(definition) { return registry.selectCharacter(definition); }
    fun chooseInitial(definition) { return registry.chooseInitialCharacter(definition); }
    fun create(definition, x, y)
    {
        // Native finite checks are required: script numeric comparison treats NaN as equal.
        if(!registry.validCharacterPosition(x, y)) return null;
        if(registry.characterModel(definition) == null || find(definition) != null) return null;
        body = Level.spawnEntity("BSL Registry Character", Vector2(x, y));
        if(!body.initialize(definition)) {
            body.destroy();
            return null;
        }
        return body;
    }
}
