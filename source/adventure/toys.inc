// Twenty-two static toys (11 original + 11 secret-room copies), one shared MDL resource. Map supplies body/skin/position.
// Solidity is an entity property, not a property stored in a studio MDL file.
new g_Toys[22];
new const TOY_MODEL[] = "models/yx_xyin9/yx_adventure_toys.mdl";

stock Toys_Init()
{
    new name[32];
    for (new i=0;i<sizeof g_Toys;i++)
    {
        formatex(name,charsmax(name),"adventure_toy_%02d",i+1);
        g_Toys[i]=Map_FindEntity(name);
    }
    Toys_Reset();
}

stock Toys_Reset()
{
    new classname[16],model[64],name[32],expected[32];
    for (new i=0;i<sizeof g_Toys;i++)
    {
        new ent=g_Toys[i];
        if (!pev_valid(ent)) continue; // Older maps without toys remain supported.
        pev(ent,pev_classname,classname,charsmax(classname));
        pev(ent,pev_model,model,charsmax(model));
        pev(ent,pev_targetname,name,charsmax(name));
        formatex(expected,charsmax(expected),"adventure_toy_%02d",i+1);
        if (!equal(classname,"cycler") || !equal(model,TOY_MODEL) || !equal(name,expected)) continue;
        set_pev(ent,pev_solid,SOLID_NOT);
        set_pev(ent,pev_movetype,MOVETYPE_NONE);
        set_pev(ent,pev_takedamage,DAMAGE_NO);
        set_pev(ent,pev_sequence,0);
        set_pev(ent,pev_frame,0.0);
        set_pev(ent,pev_framerate,0.0);
        set_pev(ent,pev_nextthink,0.0);
        new Float:origin[3];pev(ent,pev_origin,origin);
        engfunc(EngFunc_SetOrigin,ent,origin);
    }
}
