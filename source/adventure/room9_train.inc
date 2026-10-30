new g_R9DoorMotion[2][2];
// Room 9: stationary train, 5-second boarding, 36-second timed run.
// Shared room state only. No participant roster; timeout is a map trigger volume.
enum { R9_IDLE, R9_BOARDING, R9_SEALING, R9_OPENING, R9_RUNNING, R9_CLEARING };
new bool:g_R9Ready;

new g_R9State, g_R9Roof[3], g_R9Piston[3][2];
new Float:g_R9RoofHome[3][3], Float:g_R9PistonHome[3][2][3];
new Float:g_R9RoofMin[3], Float:g_R9RoofMax[3];
enum { R9_ROOF_WAIT, R9_ROOF_RISE, R9_ROOF_TRAVEL, R9_ROOF_LOWER };
new g_R9RoofPhase[3], g_R9NextRoof;
new Float:g_R9RoofLaunchAt;
new g_R9BlockedBy[33], g_R9BlockedUid[33]; // handled next frame, after engine rollback
new g_R9TimeoutTrigger;
new g_R9PistonPhase[3]; // 0=open wait, 1=closing, 2=closed wait, 3=opening
new Float:g_R9PistonAt[3], Float:g_R9Deadline, Float:g_R9PollAt, Float:g_R9FrameAt;
new const Float:R9_ZERO[3] = {0.0, 0.0, 0.0};

stock bool:Room9_Init()
{
    if (!Map_FindEntity("adventure_room9_v73")) return true; // older maps remain supported
    new doorName[48];
    for (new pair=0;pair<2;pair++) for(new leaf=0;leaf<2;leaf++)
    {
        formatex(doorName,charsmax(doorName),"room9_%s_door_%d",pair ? "departure" : "boarding",leaf);
        new ent=Map_FindEntity(doorName);
        if(!ent) { set_fail_state("Train requires v94 double doors"); return false; }
        new Float:angle = pair ? (leaf ? -90.0 : 90.0) : (leaf ? 90.0 : -90.0);
        g_R9DoorMotion[pair][leaf]=Door_Add(ent,angle,90.0,true);
    }
    new name[40];
    for (new i=0; i<3; i++)
    {
        formatex(name, charsmax(name), "room9_roof_%d", i);
        g_R9Roof[i] = Map_FindEntity(name);
        if (!g_R9Roof[i]) { set_fail_state("Room9 roof mover missing"); return false; }
        pev(g_R9Roof[i], pev_origin, g_R9RoofHome[i]);
        new Float:lo[3], Float:hi[3];
        pev(g_R9Roof[i], pev_absmin, lo); pev(g_R9Roof[i], pev_absmax, hi);
        g_R9RoofMin[i]=lo[1]+1.0; g_R9RoofMax[i]=hi[1]-1.0;
        set_pev(g_R9Roof[i], pev_movetype, MOVETYPE_PUSH);
        for (new side=0; side<2; side++)
        {
            formatex(name, charsmax(name), "room9_piston_%d_%d", i, side);
            g_R9Piston[i][side] = Map_FindEntity(name);
            if (!g_R9Piston[i][side]) { set_fail_state("Room9 piston missing"); return false; }
            pev(g_R9Piston[i][side], pev_origin, g_R9PistonHome[i][side]);
            set_pev(g_R9Piston[i][side], pev_movetype, MOVETYPE_PUSH);
        }
    }
    g_R9TimeoutTrigger=Map_FindEntity("room9_timeout_return");
    if(!g_R9TimeoutTrigger) { set_fail_state("Room9 requires v77 timeout volume"); return false; }
    Room9_SetTimeout(false);
    RegisterHam(Ham_Blocked, "func_wall", "Room9_Blocked");
    RegisterHam(Ham_Killed, "player", "Room9_Killed", 1);
    g_R9Ready=true;
    log_amx("Room9 v77 ready: zone-based timeout, no participant roster.");
    return true;
}

stock bool:Room9_Alive(id)
{
    return is_user_alive(id) && pev(id, pev_iuser1)==0;
}
stock bool:Room9_InTrainRoom(const Float:p[3])
{
    return p[0]>1728.0 && p[0]<3040.0 && p[1]>-3888.0 && p[1]<1888.0
        && p[2]>-2650.0 && p[2]<-2016.0;
}
stock bool:Room9_InFirst(const Float:p[3])
{
    return p[0]>2240.0 && p[0]<2496.0 && p[1]>-3544.0 && p[1]<-2728.0
        && p[2]>-2494.0 && p[2]<-2320.0;
}
stock bool:Room9_OnStation(const Float:p[3])
{
    // Both halls, including the arrival stairs. Finishers are not train participants.
    return p[1]>-3856.0 && p[1]<-2160.0 && p[2]>-2496.0 && p[2]<-2048.0
        && ((p[0]>1808.0 && p[0]<2224.0) || (p[0]>2624.0 && p[0]<2960.0));
}
stock Room9_SetTimeout(bool:enabled)
{
    if(!g_R9TimeoutTrigger) return;
    set_pev(g_R9TimeoutTrigger,pev_solid,enabled ? SOLID_TRIGGER : SOLID_NOT);
    new Float:p[3]; pev(g_R9TimeoutTrigger,pev_origin,p);
    engfunc(EngFunc_SetOrigin,g_R9TimeoutTrigger,p); // relink trigger after solid change
}
stock Room9_BeginClear(Float:now)
{
    g_R9State=R9_CLEARING; g_R9Deadline=now+2.0;
    for(new i=0;i<3;i++)
    {
        set_pev(g_R9Roof[i],pev_velocity,R9_ZERO);
        for(new side=0;side<2;side++) set_pev(g_R9Piston[i][side],pev_velocity,R9_ZERO);
        Room9_StopSound(g_R9Piston[i][0]);
    }
    Room9_SetTimeout(true);
}

stock bool:Room9_Return(id)
{
    // Thirty-two separated positions; actual hull test respects local non-semiclip play.
    new Float:p[3];
    for (new n=0; n<32; n++)
    {
        new slot=(id-1+n)%32;
        p[0]=2000.0+44.0*float(slot%4);
        p[1]=-3776.0+44.0*float(slot/4); p[2]=-2459.0;
        if (!Player_TryPlace(id,p)) continue;
        new Float:angle[3]; angle[1]=45.0;
        set_pev(id,pev_angles,angle); set_pev(id,pev_v_angle,angle); set_pev(id,pev_fixangle,1);
        return true;
    }
    return false; // retry next update, never force a player into occupied geometry
}
stock Room9_StopSound(ent)
{
    emit_sound(ent,CHAN_STATIC,"plats/heavymove1.wav",0.0,ATTN_NORM,SND_STOP,100);
}
stock Room9_PistonSound(pair)
{
    emit_sound(g_R9Piston[pair][0],CHAN_STATIC,"plats/heavymove1.wav",0.55,ATTN_NORM,0,100);
}
stock Room9_DoorStart(index)
{
    for(new leaf=0;leaf<2;leaf++) Door_Set(g_R9DoorMotion[index][leaf],index==1);
}
stock bool:Room9_DoorReady(index)
{
    return Door_Ready(g_R9DoorMotion[index][0]) && Door_Ready(g_R9DoorMotion[index][1]);
}

stock Room9_SetAt(ent, const Float:home[3], axis, Float:offset)
{
    new Float:p[3]; for(new a=0;a<3;a++) p[a]=home[a]; p[axis]+=offset;
    set_pev(ent,pev_velocity,R9_ZERO); engfunc(EngFunc_SetOrigin,ent,p);
}
stock Room9_Reset()
{
    if (!g_R9Ready) return;
    Room9_SetTimeout(false);

    g_R9State=R9_IDLE; g_R9PollAt=0.0; g_R9FrameAt=get_gametime();
    for (new id=1;id<=32;id++) { g_R9BlockedBy[id]=0; g_R9BlockedUid[id]=0; }
    g_R9NextRoof=0; g_R9RoofLaunchAt=0.0;
    for(new leaf=0;leaf<2;leaf++) { Door_Set(g_R9DoorMotion[0][leaf],true); Door_Set(g_R9DoorMotion[1][leaf],false); }
    for (new i=0;i<3;i++)
    {
        Room9_ParkRoof(i);
        g_R9RoofPhase[i]=R9_ROOF_WAIT;
        for(new side=0;side<2;side++) Room9_SetAt(g_R9Piston[i][side],g_R9PistonHome[i][side],0,0.0);
        Room9_StopSound(g_R9Piston[i][0]); g_R9PistonPhase[i]=0;
    }
}
// Physics moves brush entities and their passengers. Never teleport a moving solid each frame.
stock bool:Room9_MoveTo(ent, axis, Float:target, Float:speed, Float:dt)
{
    // GoldSrc SV_Physics_Pusher limits movement by nextthink - ltime.
    // A func_wall defaults to nextthink=0, so velocity alone never moves it.
    // Keep its physics window ahead; no Think callback is needed for these walls.
    new Float:localTime; pev(ent,pev_ltime,localTime);
    set_pev(ent,pev_nextthink,localTime+1.0);
    new Float:p[3],Float:v[3]; pev(ent,pev_origin,p);
    new Float:diff=target-p[axis];
    if (floatabs(diff)<0.05) { set_pev(ent,pev_velocity,R9_ZERO); return true; }
    new Float:step=floatmin(speed,floatabs(diff)/floatmax(0.001,dt));
    v[axis]=diff>0.0 ? step : -step; set_pev(ent,pev_velocity,v); return false;
}
// Recycle below the opaque roof; a single scheduler launches one lift at a time.
stock Room9_ParkRoof(i)
{
    new Float:p[3];
    for(new a=0;a<3;a++) p[a]=g_R9RoofHome[i][a];
    p[1]+=-32.0-g_R9RoofMax[i]; p[2]-=112.0;
    set_pev(g_R9Roof[i],pev_velocity,R9_ZERO);
    set_pev(g_R9Roof[i],pev_solid,SOLID_NOT);
    engfunc(EngFunc_SetOrigin,g_R9Roof[i],p);
    Entity_SetVisible(g_R9Roof[i],false);
}
stock Room9_RoofUpdate(Float:now, Float:dt)
{
    new bool:rising;
    for(new i=0;i<3;i++) if(g_R9RoofPhase[i]==R9_ROOF_RISE) rising=true;
    if(now>=g_R9RoofLaunchAt && !rising && g_R9RoofPhase[g_R9NextRoof]==R9_ROOF_WAIT)
    {
        new i=g_R9NextRoof;
        g_R9RoofPhase[i]=R9_ROOF_RISE;
        set_pev(g_R9Roof[i],pev_solid,SOLID_BSP);
        Entity_SetVisible(g_R9Roof[i],true);
        g_R9NextRoof=(i+1)%3; g_R9RoofLaunchAt=now+1.4;
    }
    for(new i=0;i<3;i++)
    {
        new ent=g_R9Roof[i];
        if(g_R9RoofPhase[i]==R9_ROOF_RISE)
        {
            if(Room9_MoveTo(ent,2,g_R9RoofHome[i][2],224.0,dt)) g_R9RoofPhase[i]=R9_ROOF_TRAVEL;
        }
        else if(g_R9RoofPhase[i]==R9_ROOF_TRAVEL)
        {
            if(Room9_MoveTo(ent,1,g_R9RoofHome[i][1]-864.0-g_R9RoofMin[i],300.0,dt))
            {
                g_R9RoofPhase[i]=R9_ROOF_LOWER;
                // Let passengers land on the carriage roof instead of dragging them into it.
                set_pev(ent,pev_solid,SOLID_NOT);
            }
        }
        else if(g_R9RoofPhase[i]==R9_ROOF_LOWER)
        {
            if(Room9_MoveTo(ent,2,g_R9RoofHome[i][2]-112.0,224.0,dt))
            {
                Room9_ParkRoof(i); g_R9RoofPhase[i]=R9_ROOF_WAIT;
            }
        }
    }
}
// This runs before engine physics, not inside Ham_Blocked (which rolls positions back).

stock Room9_ResolveBlocked()
{
    for(new id=1;id<=g_MaxPlayers;id++)
    {
        new ent=g_R9BlockedBy[id], uid=g_R9BlockedUid[id];
        g_R9BlockedBy[id]=0; g_R9BlockedUid[id]=0;
        if(!ent || !Room9_Alive(id) || get_user_userid(id)!=uid) continue;
        new Float:playerPos[3]; pev(id,pev_origin,playerPos);
        if(Room9_OnStation(playerPos) || !Room9_InTrainRoom(playerPos)) continue;
        for(new i=0;i<3;i++) if(ent==g_R9Roof[i])
        {
            new Float:p[3],Float:lo[3],Float:hi[3],Float:origin[3];
            pev(id,pev_origin,p); pev(ent,pev_absmin,lo); pev(ent,pev_absmax,hi); pev(ent,pev_origin,origin);
            if(g_R9RoofPhase[i]==R9_ROOF_RISE)
                p[2]=hi[2]-(origin[2]-g_R9RoofHome[i][2])+37.0;
            else p[1]=lo[1]-18.0;
            // No damage: free a blocked passenger above/in front of the obstacle.
            if(!Player_TryPlace(id,p)) Room9_Return(id);
        }
    }
}
stock Room9_PistonsUpdate(Float:now,Float:dt)
{
    for(new i=0;i<3;i++)
    {
        new phase=g_R9PistonPhase[i];
        if(phase==0 || phase==2)
        {
            if(now<g_R9PistonAt[i]) continue;
            phase=phase==0 ? 1 : 3; g_R9PistonPhase[i]=phase; Room9_PistonSound(i);
        }
        new bool:done=true;
        for(new side=0;side<2;side++)
        {
            new Float:offset=phase==1 ? (side==0 ? 56.0 : -56.0) : 0.0;
            if(!Room9_MoveTo(g_R9Piston[i][side],0,g_R9PistonHome[i][side][0]+offset,110.0+float(i)*35.0,dt)) done=false;
        }
        if(done)
        {
            Room9_StopSound(g_R9Piston[i][0]);
            g_R9PistonPhase[i]=phase==1 ? 2 : 0;
            g_R9PistonAt[i]=now+(phase==1 ? 0.4 : 1.4-float(i)*0.3);
        }
    }
}
stock Room9_Poll(Float:now)
{
    if(g_R9State!=R9_IDLE && g_R9State!=R9_BOARDING) return;
    new bool:occupied;
    for(new id=1;id<=g_MaxPlayers;id++) if(Room9_Alive(id))
    {
        new Float:p[3]; pev(id,pev_origin,p);
        if(Room9_InFirst(p)) { occupied=true; break; }
    }
    if(g_R9State==R9_IDLE && occupied) { g_R9State=R9_BOARDING; g_R9Deadline=now+5.0; }
    if(g_R9State==R9_BOARDING)
    {
        if(!occupied) { g_R9State=R9_IDLE; return; }
        if(now>=g_R9Deadline) { g_R9State=R9_SEALING; Room9_DoorStart(0); }
    }
}
stock Room9_OnFrame(Float:now)
{
    if(!g_R9Ready) return;
    // Activate native timeout zones on the first frame at/after 36 seconds.
    if(g_R9State==R9_RUNNING && now>=g_R9Deadline) Room9_BeginClear(now);
    if(g_R9State==R9_CLEARING)
    {
        if(now>=g_R9Deadline) Room9_Reset();
        return;
    }
    Room9_ResolveBlocked();
    new Float:dt=floatclamp(now-g_R9FrameAt,0.001,0.05); g_R9FrameAt=now;
    if(now>=g_R9PollAt) { g_R9PollAt=now+0.1; Room9_Poll(now); }
    if(g_R9State==R9_SEALING)
    {
        if(Room9_DoorReady(0))
        {
            g_R9State=R9_OPENING; Room9_DoorStart(1);
        }
    }
    if(g_R9State==R9_OPENING)
    {
        if(Room9_DoorReady(1))
        {
            g_R9State=R9_RUNNING; g_R9Deadline=now+36.0;
            g_R9RoofLaunchAt=now; g_R9NextRoof=0;
            for(new i=0;i<3;i++) { g_R9PistonPhase[i]=0; g_R9PistonAt[i]=now+float(i)*0.5+0.5; }
        }
    }
    if(g_R9State==R9_RUNNING) { Room9_RoofUpdate(now,dt); Room9_PistonsUpdate(now,dt); }
}
public Room9_Killed(id)
{
    if(g_R9Ready && id>=1 && id<=32) { g_R9BlockedBy[id]=0; g_R9BlockedUid[id]=0; }
    return HAM_IGNORED;
}
public Room9_Blocked(ent,other)
{
    if(!g_R9Ready) return HAM_IGNORED;
    if(other<1 || other>g_MaxPlayers || !Room9_Alive(other)) return HAM_IGNORED;
    for(new i=0;i<3;i++)
    {
        if(ent==g_R9Roof[i])
        {
            g_R9BlockedBy[other]=ent; g_R9BlockedUid[other]=get_user_userid(other);
            return HAM_SUPERCEDE;
        }
        if((ent==g_R9Piston[i][0] || ent==g_R9Piston[i][1]) && g_R9State==R9_RUNNING)
        {
            if(g_R9PistonPhase[i]==1) Player_Kill(other);
            return HAM_SUPERCEDE;
        }
    }
    return HAM_IGNORED;
}

