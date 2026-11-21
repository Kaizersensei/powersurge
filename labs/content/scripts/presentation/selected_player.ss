// Resolve the selected definition through the existing character factory. Signed: President.
using SurgeEngine.Transform;
object "BSL Selected Player Spawn" is "entity", "awake", "private"
{
    transform=Transform();
    characters=spawn("BSL Character Registry");
    public result="pending";
    state "main"
    {
        id=characters.selectedDefinition();
        if(id==null) { Console.print("Selected character unavailable.");result="failed";state="failed";return; }
        position=transform.position;
        if(characters.create(id,position.x,position.y)==null) {
            Console.print("Selected character could not spawn safely.");result="failed";state="failed";return;
        }
        result="spawned";state="done";
    }
    state "done" {}
    state "failed" {}
}
