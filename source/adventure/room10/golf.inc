// Room 10: one shared Scout golf ball. No player roster or saved player progress.
// Fixed-step kinematic integration; only this entity is moved, no server cvars changed.
#include "adventure/room10/layout.inc"

new bool:g_R10Map, bool:g_R10Ready, bool:g_R10Complete, bool:g_R10Moving;
new g_R10Ball, g_R10Bag, g_R10Button[3], g_R10Portal[3], g_R10Rotor, g_R10Trace, g_R10Gravity;
new g_R10Stage;
new g_R10Rotor2, g_R10ReturnObstacle, g_R10Hit;
new Float:g_R10SandSince;
new Float:g_R10Pos[3], Float:g_R10Velocity[3];
new Float:g_R10Clock, Float:g_R10Carry, Float:g_R10PollAt, Float:g_R10Transition;
new Float:g_R10UseAt;
new const Float:R10_ZERO[3] = {0.0,0.0,0.0};
new const Float:R10_MIN[3] = {-8.0,-8.0,-8.0};
new const Float:R10_MAX[3] = {8.0,8.0,8.0};
new const R10_MODEL[] = "models/yx_xyin9/yx_golf_ball.mdl";
const Float:R10_DT=0.02;
const Float:R10_SHOT_SPEED=650.0;

stock Room10_Precache()
{
    if (!g_R10Map) return;
    if (!file_exists(R10_MODEL)) { set_fail_state("Room10 requires models/yx_xyin9/yx_golf_ball.mdl"); return; }
    precache_model(R10_MODEL);
    new e=engfunc(EngFunc_CreateNamedEntity,engfunc(EngFunc_AllocString,"weapon_scout"));
    if (pev_valid(e)) { ExecuteHam(Ham_Precache,e); engfunc(EngFunc_RemoveEntity,e); }
}
stock bool:Room10_Init()
{
    if (!g_R10Map) return true;
    g_R10Bag=Map_FindEntity("room10_scout_bag");
    g_R10Portal[0]=Map_FindEntity("room10_exit_frame");
    g_R10Portal[1]=Map_FindEntity("room10_exit_vortex");
    g_R10Portal[2]=Map_FindEntity("room10_exit_trigger");
    g_R10Rotor=Map_FindEntity("room10_rotor");
    g_R10Rotor2=Map_FindEntity("room10_rotor_2");
    g_R10ReturnObstacle=Map_FindEntity("room10_reset_obstacles");
    new name[40];
    for(new i=0;i<3;i++) { formatex(name,charsmax(name),"room10_reset_%d",i+1);g_R10Button[i]=Map_FindEntity(name); }
    if(!g_R10Bag || !g_R10Rotor || !g_R10Rotor2 || !g_R10ReturnObstacle || !g_R10Portal[0] || !g_R10Portal[1] || !g_R10Portal[2]
        || !g_R10Button[0] || !g_R10Button[1] || !g_R10Button[2])
    { set_fail_state("Room10 requires map v85 or newer (golf entities missing)");return false; }
    g_R10Ball=engfunc(EngFunc_CreateNamedEntity,engfunc(EngFunc_AllocString,"info_target"));
    if(!pev_valid(g_R10Ball)) {set_fail_state("Cannot create shared golf ball");return false;}
    ExecuteHam(Ham_Spawn,g_R10Ball);
    RegisterHamFromEntity(Ham_TraceAttack,g_R10Ball,"Room10_TraceAttack");
    RegisterHamFromEntity(Ham_TakeDamage,g_R10Ball,"Room10_TakeDamage");
    set_pev(g_R10Ball,pev_classname,"adventure_golf_ball");
    engfunc(EngFunc_SetModel,g_R10Ball,R10_MODEL);
    engfunc(EngFunc_SetSize,g_R10Ball,R10_MIN,R10_MAX);
    set_pev(g_R10Ball,pev_movetype,MOVETYPE_NONE);
    set_pev(g_R10Ball,pev_health,100000.0);
    set_pev(g_R10Ball,pev_takedamage,DAMAGE_YES);
    set_pev(g_R10Ball,pev_framerate,0.0);
    set_pev(g_R10Ball,pev_nextthink,0.0);
    g_R10Trace=create_tr2();g_R10Gravity=get_cvar_pointer("sv_gravity");
    g_R10Ready=true;
    return true;
}
stock Room10_SetPortal(bool:on)
{
    for(new i=0;i<2;i++) Entity_SetVisible(g_R10Portal[i],on);
    set_pev(g_R10Portal[2],pev_solid,on?SOLID_TRIGGER:SOLID_NOT);
    new Float:p[3];pev(g_R10Portal[2],pev_origin,p);engfunc(EngFunc_SetOrigin,g_R10Portal[2],p);
}
stock Room10_SetRotor(bool:on)
{
    ExecuteHamB(Ham_Use,g_R10Rotor,0,0,on?USE_ON:USE_OFF,0.0);
    if(!on) {set_pev(g_R10Rotor,pev_avelocity,R10_ZERO);set_pev(g_R10Rotor,pev_angles,R10_ZERO);}
    if(pev_valid(g_R10Rotor2))
    {
        ExecuteHamB(Ham_Use,g_R10Rotor2,0,0,on?USE_ON:USE_OFF,0.0);
        if(!on) {set_pev(g_R10Rotor2,pev_avelocity,R10_ZERO);set_pev(g_R10Rotor2,pev_angles,R10_ZERO);}
    }
}
stock Room10_PaintButtons()
{
    for(new i=0;i<3;i++)
    {
        // Native +0/+A texture families retain the room's button artwork.
        set_pev(g_R10Button[i],pev_frame,(!g_R10Complete && i==g_R10Stage)?1.0:0.0);
    }
}
stock Room10_PlaceBall()
{
    if(g_R10Complete) return;
    for(new i=0;i<3;i++) {g_R10Pos[i]=R10_TEE[g_R10Stage][i];g_R10Velocity[i]=0.0;}
    set_pev(g_R10Ball,pev_velocity,R10_ZERO);set_pev(g_R10Ball,pev_angles,R10_ZERO);
    set_pev(g_R10Ball,pev_solid,SOLID_BBOX);Entity_SetVisible(g_R10Ball,true);
    engfunc(EngFunc_SetOrigin,g_R10Ball,g_R10Pos);
    g_R10Moving=true; // settle onto the tee through the same collision solver
    g_R10Carry=0.0;g_R10Transition=0.0;g_R10PollAt=0.0;
    g_R10SandSince=0.0;
}
stock Room10_Reset()
{
    if(!g_R10Ready) return;
    g_R10Stage=0;g_R10Complete=false;g_R10Transition=0.0;g_R10UseAt=0.0;
    g_R10Clock=get_gametime();Room10_SetPortal(false);Room10_SetRotor(false);
    Room10_PlaceBall();Room10_PaintButtons();
}
stock Room10_Shutdown()
{
    if(g_R10Trace) {free_tr2(g_R10Trace);g_R10Trace=0;}
    if(pev_valid(g_R10Ball)) engfunc(EngFunc_RemoveEntity,g_R10Ball);
    g_R10Ready=false;
}
stock Room10_Score(Float:now)
{
    if(g_R10Complete || g_R10Transition>0.0) return;
    g_R10Moving=false;set_pev(g_R10Ball,pev_solid,SOLID_NOT);Entity_SetVisible(g_R10Ball,false);
    emit_sound(g_R10Ball,CHAN_ITEM,"buttons/bell1.wav",0.75,ATTN_NORM,0,100);
    if(g_R10Stage<2) {g_R10Stage++;g_R10Transition=now+0.8;}
    else {g_R10Complete=true;Room10_SetPortal(true);Room10_SetRotor(false);}
    Room10_PaintButtons();
}
stock bool:Room10_CanUse(ent,id)
{
    return is_user_alive(id) && pev(id,pev_iuser1)==0 && (pev(id,pev_button)&IN_USE)
        && Player_IsAimingAt(id,ent);
}
stock Room10_Use(ent,id)
{
    if(!g_R10Ready) return HAM_IGNORED;
    new index=-1;
    for(new i=0;i<3;i++) if(ent==g_R10Button[i]) index=i;
    if(ent!=g_R10Bag && index<0) return HAM_IGNORED;
    if(!Room10_CanUse(ent,id)) return HAM_SUPERCEDE;
    if(index>=0 && (g_R10Complete || index!=g_R10Stage || g_R10Transition>0.0)) return HAM_SUPERCEDE;
    new Float:now=get_gametime();if(now<g_R10UseAt) return HAM_SUPERCEDE;
    g_R10UseAt=now+0.5;
    if(ent==g_R10Bag) Room10_GiveScout(id);
    else {Room10_PlaceBall();emit_sound(ent,CHAN_ITEM,"buttons/button3.wav",0.6,ATTN_NORM,0,100);}
    return HAM_SUPERCEDE;
}
stock Room10_GiveScout(id)
{
    if(!user_has_weapon(id,CSW_SCOUT))
    {
        new weapons[32],count,name[24];get_user_weapons(id,weapons,count);
        for(new i=0;i<count;i++)
        {
            switch(weapons[i])
            {
                case CSW_XM1014,CSW_MAC10,CSW_AUG,CSW_UMP45,CSW_SG550,CSW_GALIL,CSW_FAMAS,
                     CSW_AWP,CSW_MP5NAVY,CSW_M249,CSW_M3,CSW_M4A1,CSW_TMP,CSW_G3SG1,CSW_SG552,CSW_AK47,CSW_P90:
                {get_weaponname(weapons[i],name,charsmax(name));engclient_cmd(id,"drop",name);}
            }
        }
        new e=engfunc(EngFunc_CreateNamedEntity,engfunc(EngFunc_AllocString,"weapon_scout"));
        if(pev_valid(e))
        {
            set_pev(e,pev_spawnflags,1<<30);ExecuteHamB(Ham_Spawn,e);
            new Float:p[3];pev(id,pev_origin,p);engfunc(EngFunc_SetOrigin,e,p);ExecuteHamB(Ham_Touch,e,id);
            if(pev_valid(e) && pev(e,pev_owner)!=id) engfunc(EngFunc_RemoveEntity,e);
        }
    }
    if(user_has_weapon(id,CSW_SCOUT)) ExecuteHamB(Ham_GiveAmmo,id,90,"762nato",90);
}
public Room10_TraceAttack(ent,attacker,Float:damage,Float:direction[3],trace,damageBits)
{
    if(!g_R10Ready || ent!=g_R10Ball) return HAM_IGNORED;
    if(g_R10Complete || g_R10Transition>0.0 || !is_user_alive(attacker) || pev(attacker,pev_iuser1)!=0
        || get_user_weapon(attacker)!=CSW_SCOUT) return HAM_SUPERCEDE;
    new Float:len=floatsqroot(direction[0]*direction[0]+direction[1]*direction[1]);
    if(len<0.01) return HAM_SUPERCEDE;
    g_R10Velocity[0]=direction[0]/len*R10_SHOT_SPEED;g_R10Velocity[1]=direction[1]/len*R10_SHOT_SPEED;
    g_R10Velocity[2]=0.0;g_R10Moving=true;
    g_R10SandSince=0.0;
    return HAM_SUPERCEDE; // ball is hit, not damaged or removed
}
public Room10_TakeDamage(ent,inflictor,attacker,Float:damage,damageBits)
{
    return (g_R10Ready && ent==g_R10Ball)?HAM_SUPERCEDE:HAM_IGNORED;
}
stock Float:Room10_Dot(const Float:a[3],const Float:b[3])
{
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
// Sweep the ball's 16-unit collision box using its eight corners plus center.
// IGNORE_MONSTERS excludes players; do NOT use IGNORE_GLASS: invisible rock hulls are solid.
stock Float:Room10_Sweep(const Float:a[3],const Float:b[3],Float:normal[3], &bool:embedded)
{
    new Float:best=1.0;embedded=false;g_R10Hit=0;
    for(new axis=0;axis<3;axis++) normal[axis]=0.0;
    for(new i=0;i<9;i++)
    {
        new Float:start[3],Float:finish[3];
        for(new k=0;k<3;k++)
        {
            new Float:offset=(i==8)?0.0:((i&(1<<k))?8.0:-8.0);
            start[k]=a[k]+offset;finish[k]=b[k]+offset;
        }
        engfunc(EngFunc_TraceLine,start,finish,IGNORE_MONSTERS,g_R10Ball,g_R10Trace);
        if(get_tr2(g_R10Trace,TR_StartSolid)) {embedded=true;return 0.0;}
        new Float:f;get_tr2(g_R10Trace,TR_flFraction,f);
        if(f<best) {best=f;get_tr2(g_R10Trace,TR_vecPlaneNormal,normal);g_R10Hit=get_tr2(g_R10Trace,TR_pHit);}
    }
    return best;
}
stock Room10_GroundForces(const Float:normal[3],Float:gravity,Float:dt,bool:sand)
{
    new Float:into=Room10_Dot(g_R10Velocity,normal);
    for(new i=0;i<3;i++) g_R10Velocity[i]-=normal[i]*into;
    // Tangential gravity accelerates downhill and slows uphill without a fake step lift.
    for(new i=0;i<3;i++) g_R10Velocity[i]+=gravity*normal[2]*normal[i]*dt;
    g_R10Velocity[2]-=gravity*dt;
    new Float:speed=floatsqroot(Room10_Dot(g_R10Velocity,g_R10Velocity));
    if(speed>0.0)
    {
        new Float:drag=(sand?110.0:40.0)+(sand?0.004:0.0025)*speed*speed;
        new Float:factor=floatmax(0.0,1.0-drag*dt/speed);
        for(new i=0;i<3;i++) g_R10Velocity[i]*=factor;
    }
}
stock Room10_Bounce(const Float:normal[3])
{
    new Float:dot=Room10_Dot(g_R10Velocity,normal);
    if(dot>=0.0) return;
    new Float:restitution=(normal[2]>0.65)?(dot< -110.0?0.15:0.0):0.65;
    for(new i=0;i<3;i++) g_R10Velocity[i]-=(1.0+restitution)*dot*normal[i];
}
// Cross tiny material seams/tee inlays only. Full-size obstacles and rocks stay walls.
stock bool:Room10_TrySeam(const Float:dest[3])
{
    new Float:up[3],Float:over[3],Float:down[3],Float:n[3],bool:embedded;
    for(new i=0;i<3;i++) {up[i]=g_R10Pos[i];over[i]=dest[i];down[i]=dest[i];}
    up[2]+=2.0;over[2]+=2.0;down[2]-=1.0;
    if(Room10_Sweep(g_R10Pos,up,n,embedded)<1.0 || embedded) return false;
    if(Room10_Sweep(up,over,n,embedded)<1.0 || embedded) return false;
    new Float:f=Room10_Sweep(over,down,n,embedded);
    if(embedded || f>=1.0 || n[2]<0.65) return false;
    for(new i=0;i<3;i++) g_R10Pos[i]=over[i]+(down[i]-over[i])*f;
    g_R10Pos[2]+=0.04/n[2];
    return true;
}
// Small local correction for numerical contact errors, never an arbitrary lateral jump.
// Most importantly, normal movement below does NOT push a grounded ball sideways
// into the downhill curb. A truly trapped ball still returns to the active tee.
stock bool:Room10_RecoverContact()
{
    new Float:p[3],Float:n[3],bool:embedded;
    for(new attempt=0;attempt<4;attempt++)
    {
        for(new i=0;i<3;i++) p[i]=g_R10Pos[i];
        p[2]+=0.125*float(1<<attempt);
        Room10_Sweep(p,p,n,embedded);
        if(!embedded)
        {
            for(new i=0;i<3;i++) g_R10Pos[i]=p[i];
            return true;
        }
    }
    return false;
}
stock Float:Room10_FloorZ(Float:y)
{
    return y<=-2304.0?-2239.5:(y>=-1792.0?-2143.5:-2239.5+(y+2304.0)*0.1875);
}
// 1=sand, 2=water. Exact polygons are shared with the map generator.
// Height gating allows a launched ball to fly OVER the water safely.
stock Room10_SurfaceUncached()
{
    if(g_R10Pos[2]>Room10_FloorZ(g_R10Pos[1])+8.75) return 0;
    for(new j=0;j<sizeof R10_HAZARD_POLY;j++)
    {
        if(R10_HAZARD_TYPE[j]==1 && g_R10Stage!=0) continue;
        new bool:positive=false,bool:negative=false;
        for(new k=0;k<4;k++)
        {
            new a=k*2,b=((k+1)%4)*2;
            new Float:cross=(R10_HAZARD_POLY[j][b]-R10_HAZARD_POLY[j][a])*(g_R10Pos[1]-R10_HAZARD_POLY[j][a+1])
                -(R10_HAZARD_POLY[j][b+1]-R10_HAZARD_POLY[j][a+1])*(g_R10Pos[0]-R10_HAZARD_POLY[j][a]);
            if(cross>0.01) positive=true;
            if(cross< -0.01) negative=true;
        }
        if(!(positive && negative)) return R10_HAZARD_TYPE[j];
    }
    return 0;
}
stock Room10_CheckSand(Float:now)
{
    if(Room10_Dot(g_R10Velocity,g_R10Velocity)>0.0001 || Room10_Surface()!=1)
    {g_R10SandSince=0.0;return;}
    if(g_R10SandSince==0.0) g_R10SandSince=now;
    else if(now-g_R10SandSince>=3.0) Room10_PlaceBall();
}
stock bool:Room10_InZone(zone,Float:alongLimit,Float:acrossLow,Float:acrossHigh)
{
    new Float:dx=g_R10Pos[0]-R10_SURFACE_ZONE[zone][0],Float:dy=g_R10Pos[1]-R10_SURFACE_ZONE[zone][1];
    new Float:a=dx*R10_SURFACE_ZONE[zone][2]+dy*R10_SURFACE_ZONE[zone][3];
    new Float:b=floatabs(-dx*R10_SURFACE_ZONE[zone][3]+dy*R10_SURFACE_ZONE[zone][2]);
    return floatabs(a)<alongLimit && b>=acrossLow && b<acrossHigh;
}
stock bool:Room10_OnCourse()
{
    if(g_R10Pos[2]< -2330.0 || g_R10Pos[2]> -1500.0) return false;
    for(new i=R10_SEGMENT_START[g_R10Stage];i<R10_SEGMENT_START[g_R10Stage+1];i++)
    {
        new Float:dx=R10_SEGMENTS[i][2]-R10_SEGMENTS[i][0],Float:dy=R10_SEGMENTS[i][3]-R10_SEGMENTS[i][1];
        new Float:t=floatclamp(((g_R10Pos[0]-R10_SEGMENTS[i][0])*dx+(g_R10Pos[1]-R10_SEGMENTS[i][1])*dy)/(dx*dx+dy*dy),0.0,1.0);
        dx=g_R10Pos[0]-R10_SEGMENTS[i][0]-t*dx;dy=g_R10Pos[1]-R10_SEGMENTS[i][1]-t*dy;
        new Float:r=R10_SEGMENTS[i][4]+16.0;
        if(dx*dx+dy*dy<r*r) return true;
    }
    return false;
}
stock bool:Room10_InCup()
{
    new Float:dx=g_R10Pos[0]-R10_CUP[g_R10Stage][0],Float:dy=g_R10Pos[1]-R10_CUP[g_R10Stage][1];
    return dx*dx+dy*dy<100.0 && g_R10Pos[2]<R10_CUP[g_R10Stage][2]-3.0;
}
stock Room10_Step(Float:now)
{
    new Float:down[3],Float:normal[3],bool:embedded;
    for(new i=0;i<3;i++) down[i]=g_R10Pos[i];down[2]-=3.0;
    new Float:f=Room10_Sweep(g_R10Pos,down,normal,embedded);
    if(embedded) {if(!Room10_RecoverContact()) Room10_PlaceBall();return;}
    if(f<1.0 && g_R10ReturnObstacle && g_R10Hit==g_R10ReturnObstacle) {Room10_PlaceBall();return;}
    new bool:grounded=(f<1.0 && normal[2]>0.65 && Room10_Dot(g_R10Velocity,normal)<25.0);
    if(grounded)
    {
        g_R10Pos[2]-=3.0*f;
        // Vertical clearance follows the floor without crossing a neighbouring wall.
        g_R10Pos[2]+=0.04/normal[2];
        engfunc(EngFunc_SetOrigin,g_R10Ball,g_R10Pos);
        if(Room10_Surface()==2) {Room10_PlaceBall();return;}
        Room10_GroundForces(normal,get_pcvar_float(g_R10Gravity),R10_DT,Room10_Surface()==1);
        if(normal[2]>0.999 && Room10_Dot(g_R10Velocity,g_R10Velocity)<1.0)
        {g_R10Moving=false;for(new i=0;i<3;i++)g_R10Velocity[i]=0.0;return;}
    }
    else g_R10Velocity[2]-=get_pcvar_float(g_R10Gravity)*R10_DT;
    new Float:left=R10_DT;
    for(new bump=0;bump<4 && left>0.0001;bump++)
    {
        new Float:dest[3];for(new i=0;i<3;i++) dest[i]=g_R10Pos[i]+g_R10Velocity[i]*left;
        f=Room10_Sweep(g_R10Pos,dest,normal,embedded);
        if(embedded) {if(!Room10_RecoverContact()) Room10_PlaceBall();return;}
        if(f<1.0 && g_R10ReturnObstacle && g_R10Hit==g_R10ReturnObstacle) {Room10_PlaceBall();return;}
        if(grounded && f<1.0 && normal[2]<0.65 && Room10_TrySeam(dest)) break;
        // Leave clearance along the already swept safe segment. Pushing along a
        // wall normal can otherwise push the box into an adjacent sloping floor.
        new Float:distance=0.0;
        for(new i=0;i<3;i++) distance+=(dest[i]-g_R10Pos[i])*(dest[i]-g_R10Pos[i]);
        distance=floatsqroot(distance);
        new Float:travel=(f<1.0 && distance>0.0001)?floatmax(0.0,f-0.04/distance):f;
        for(new i=0;i<3;i++) g_R10Pos[i]+=(dest[i]-g_R10Pos[i])*travel;
        if(f>=1.0) break;
        Room10_Bounce(normal);left*=1.0-f;
    }
    engfunc(EngFunc_SetOrigin,g_R10Ball,g_R10Pos);
    if(Room10_InCup()) {Room10_Score(now);return;}
    if(Room10_Surface()==2) {Room10_PlaceBall();return;}
    if(now>=g_R10PollAt)
    {
        g_R10PollAt=now+0.2;
        if(!Room10_OnCourse()) Room10_PlaceBall();
    }
}
stock Room10_OnFrame(Float:now)
{
    if(!g_R10Ready) return;
    new Float:elapsed=floatclamp(now-g_R10Clock,0.0,0.1);g_R10Clock=now;
    if(g_R10Complete) return;
    if(g_R10Transition>0.0)
    {
        if(now>=g_R10Transition) {Room10_PlaceBall();Room10_SetRotor(g_R10Stage==2);}
        return;
    }
    Room10_CheckSand(now); // must run even while a stationary ball sleeps
    if(!g_R10Moving) {g_R10Carry=0.0;return;}
    g_R10Carry+=elapsed;
    for(new step=0;step<5 && g_R10Carry>=R10_DT;step++)
    {
        g_R10Carry-=R10_DT;Room10_Step(now);
        if(!g_R10Moving || g_R10Complete || g_R10Transition>0.0) break;
    }
}

new bool:g_R10SurfaceCached;
new g_R10SurfaceStage, g_R10SurfaceValue;
new Float:g_R10SurfacePosition[3];
stock Room10_Surface()
{
    if (g_R10SurfaceCached && g_R10SurfaceStage==g_R10Stage
        && g_R10SurfacePosition[0]==g_R10Pos[0] && g_R10SurfacePosition[1]==g_R10Pos[1]
        && g_R10SurfacePosition[2]==g_R10Pos[2]) return g_R10SurfaceValue;
    for (new a=0;a<3;a++) g_R10SurfacePosition[a]=g_R10Pos[a];
    g_R10SurfaceStage=g_R10Stage;
    g_R10SurfaceValue=Room10_SurfaceUncached();
    g_R10SurfaceCached=true;
    return g_R10SurfaceValue;
}
