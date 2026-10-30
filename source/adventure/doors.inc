// Shared native brush motion. SetOrigin is used only for reset/end-point correction.
// Engine velocity/avelocity provides continuous movement and client interpolation.
#define DOOR_LIMIT 16
new bool:g_DoorSnapshotActive;
new g_DoorSnapshotValid[DOOR_LIMIT];
new Float:g_DoorSnapshotDistance[DOOR_LIMIT];
new bool:g_DoorConceal[DOOR_LIMIT];
new g_DoorCount, g_DoorEnt[DOOR_LIMIT];
new bool:g_DoorAngular[DOOR_LIMIT], bool:g_DoorOpen[DOOR_LIMIT], bool:g_DoorMoving[DOOR_LIMIT];
new Float:g_DoorHome[DOOR_LIMIT][3], Float:g_DoorTravel[DOOR_LIMIT], Float:g_DoorSpeed[DOOR_LIMIT];
new const Float:DOOR_ZERO[3] = {0.0, 0.0, 0.0};

stock Door_Add(ent, Float:travel, Float:speed, bool:angular = false, bool:conceal = false)
{
    if (!pev_valid(ent) || g_DoorCount >= DOOR_LIMIT) { set_fail_state("Door layout missing: use map v94 and matching plugin"); return -1; }
    new d = g_DoorCount++;
    g_DoorConceal[d] = conceal;
    g_DoorEnt[d] = ent; g_DoorTravel[d] = travel; g_DoorSpeed[d] = speed; g_DoorAngular[d] = angular;
    pev(ent, pev_origin, g_DoorHome[d]);
    set_pev(ent, pev_movetype, MOVETYPE_PUSH);
    set_pev(ent, pev_solid, SOLID_BSP);
    return d;
}

stock Door_Set(d, bool:open, bool:snap = false)
{
    if (g_DoorOpen[d] != open || snap) g_DoorSnapshotValid[d] = 0;
    g_DoorOpen[d] = open;
    if (!snap) return;
    new ent = g_DoorEnt[d], Float:p[3];
    set_pev(ent, pev_velocity, DOOR_ZERO); set_pev(ent, pev_avelocity, DOOR_ZERO);
    set_pev(ent, pev_nextthink, 0.0);
    if (g_DoorAngular[d])
    {
        p[1] = open ? g_DoorTravel[d] : 0.0; set_pev(ent, pev_angles, p);
        engfunc(EngFunc_SetOrigin,ent,g_DoorHome[d]); // Relink rotated bounds after reset.
    }
    else { for (new a=0;a<3;a++) p[a]=g_DoorHome[d][a]; p[2]+=open ? g_DoorTravel[d] : 0.0; engfunc(EngFunc_SetOrigin,ent,p); }
    emit_sound(ent,CHAN_STATIC,"doors/doormove1.wav",0.0,ATTN_NORM,SND_STOP,100);
    g_DoorMoving[d]=false;
    if (g_DoorConceal[d]) Entity_SetVisible(ent, !open);
}

stock Float:Door_Distance(d)
{
    if (g_DoorSnapshotActive && g_DoorSnapshotValid[d]) return g_DoorSnapshotDistance[d];
    new Float:p[3];
    pev(g_DoorEnt[d],g_DoorAngular[d] ? pev_angles : pev_origin,p);
    new Float:distance = (g_DoorOpen[d] ? g_DoorTravel[d] : 0.0) - (g_DoorAngular[d] ? p[1] : p[2]-g_DoorHome[d][2]);
    if (g_DoorSnapshotActive) { g_DoorSnapshotDistance[d] = distance; g_DoorSnapshotValid[d] = 1; }
    return distance;
}
stock bool:Door_Ready(d) { return floatabs(Door_Distance(d)) < 0.02; }

stock Doors_Frame()
{
    for (new d=0;d<g_DoorCount;d++)
    {
        new ent=g_DoorEnt[d], Float:delta=Door_Distance(d), Float:v[3], Float:lt;
        if (floatabs(delta)<0.02)
        {
            if (g_DoorMoving[d])
            {
                Door_Set(d,g_DoorOpen[d],true);
                emit_sound(ent,CHAN_ITEM,"doors/doorstop1.wav",0.55,ATTN_NORM,0,100);
            }
            continue;
        }
        if (g_DoorConceal[d]) Entity_SetVisible(ent, true);
        if (!g_DoorMoving[d]) emit_sound(ent,CHAN_STATIC,"doors/doormove1.wav",0.6,ATTN_NORM,0,100);
        g_DoorMoving[d]=true;
        v[g_DoorAngular[d] ? 1 : 2]=(delta>0.0 ? 1.0 : -1.0)*g_DoorSpeed[d];
        set_pev(ent,g_DoorAngular[d] ? pev_avelocity : pev_velocity,v);
        // Pusher physics stops exactly at nextthink, including a short final frame.
        pev(ent,pev_ltime,lt); set_pev(ent,pev_nextthink,lt+floatabs(delta)/g_DoorSpeed[d]);
    }
}
stock Doors_SnapAll() { for(new d=0;d<g_DoorCount;d++) Door_Set(d,g_DoorOpen[d],true); }
stock Doors_Stop() { for(new d=0;d<g_DoorCount;d++) { set_pev(g_DoorEnt[d],pev_velocity,DOOR_ZERO); set_pev(g_DoorEnt[d],pev_avelocity,DOOR_ZERO); emit_sound(g_DoorEnt[d],CHAN_STATIC,"doors/doormove1.wav",0.0,ATTN_NORM,SND_STOP,100); g_DoorMoving[d]=false; } }

public Doors_Blocked(ent,other)
{
    for(new d=0;d<g_DoorCount;d++) if(ent==g_DoorEnt[d])
    {
        if(other>=1 && other<=g_MaxPlayers && is_user_alive(other)) Player_Kill(other);
        g_DoorSnapshotValid[d] = 0;
        // No reversing or reopening on obstruction. Physics retries after rollback.
        return HAM_SUPERCEDE;
    }
    return HAM_IGNORED;
}

stock Doors_BeginSnapshot()
{
    arrayset(g_DoorSnapshotValid, 0, sizeof g_DoorSnapshotValid);
    g_DoorSnapshotActive = true;
}
