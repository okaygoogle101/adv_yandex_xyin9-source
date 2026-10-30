// Exact box/hole contact using player bounds, including crouching and the thin
// decorative rim. No extra trigger models and no traces/searches per player.
#include "adventure/room11/contact_layout.inc"
new g_R11ContactIndex[sizeof R11_CONTACT_GATES][4];

stock bool:Room11_ContactsGateAt(id, bool:flight, const Float:p[3])
{
    new Float:mins[3],Float:maxs[3],Float:lo[3],Float:hi[3];
    pev(id,pev_mins,mins);pev(id,pev_maxs,maxs);
    // GoldSrc stops a hull a small epsilon before the physical surface.
    for (new a=0;a<3;a++) {lo[a]=p[a]+mins[a]-0.1;hi[a]=p[a]+maxs[a]+0.1;}
    new first=flight?6:0,last=flight?sizeof R11_CONTACT_GATES:6;
    for (new i=first;i<last;i++)
    {
        new n=g_R11ContactIndex[i][0];
        new u=g_R11ContactIndex[i][1],v=g_R11ContactIndex[i][2];
        new Float:offset[3],m=g_R11ContactIndex[i][3];
        if (m>=0)
        {
            new ent=flight?g_R11FMover[m]:g_R11Mover[m];
            pev(ent,pev_origin,offset);
            for (new a=0;a<3;a++) offset[a]-=flight?g_R11FHome[m][a]:g_R11Home[m][a];
        }
        new Float:near=R11_CONTACT_GATES[i][3]+offset[n];
        new Float:far=R11_CONTACT_GATES[i][4]+offset[n];
        if (hi[n]<near-R11_CONTACT_GATES[i][9] || lo[n]>far+R11_CONTACT_GATES[i][10]) continue;
        new Float:left=R11_CONTACT_GATES[i][5]+offset[u]-R11_CONTACT_GATES[i][7];
        new Float:right=left+2.0*R11_CONTACT_GATES[i][7];
        new Float:bottom=R11_CONTACT_GATES[i][6]+offset[v]-R11_CONTACT_GATES[i][8];
        new Float:top=bottom+2.0*R11_CONTACT_GATES[i][8];
        if (lo[u]>left && hi[u]<right && lo[v]>bottom && hi[v]<top) continue;
        // Inside the slab: all space outside the hole is solid. Outside its
        // thickness: only the eight-unit rim is a contact surface.
        if (hi[n]>=near && lo[n]<=far) return true;
        if (hi[u]>=left-8.0 && lo[u]<=right+8.0 && hi[v]>=bottom-8.0 && lo[v]<=top+8.0) return true;
    }
    return false;
}

stock Room11Contact_Init()
{
    if (!g_R11Ready && !g_R11FReady) return;
    for (new i=0;i<sizeof R11_CONTACT_GATES;i++)
    {
        for (new a=0;a<3;a++) g_R11ContactIndex[i][a]=floatround(R11_CONTACT_GATES[i][a]);
        g_R11ContactIndex[i][3]=floatround(R11_CONTACT_GATES[i][11]);
    }
    register_forward(FM_PlayerPreThink,"Room11Contact_PreThink");
    register_forward(FM_PlayerPostThink,"Room11Contact_PostThink",1);
    register_forward(FM_Touch,"Room11Contact_Touch");
}

public Room11Contact_PostThink(id)
{
    if (!g_PluginReady) return FMRES_IGNORED;
    new Float:p[3], zone=Room11_PlayerZone(id,p);
    if (g_R11Ready && zone==1 && Room11_ContactsGateAt(id,false,p)) Room11_Return(id);
    else if (g_R11FReady && zone==2)
    {
        if (Room11_ContactsGateAt(id,true,p)) Room11Flight_Return(id);
        else Room11Flight_CheckFinishAt(id,p);
    }
    return FMRES_IGNORED;
}

public Room11Contact_Touch(ent,other)
{
    if (!g_PluginReady) return FMRES_IGNORED;
    new id=other,obstacle=ent;
    if (ent>=1 && ent<=g_MaxPlayers) {id=ent;obstacle=other;}
    if (id<1 || id>g_MaxPlayers) return FMRES_IGNORED;
    // Reject unrelated touches before querying any player properties.
    new obstacleZone=-1;
    if (obstacle==0) obstacleZone=0;
    else
    {
        if (g_R11Ready)
        {
            if (obstacle==g_R11Static) obstacleZone=1;
            for (new i=0;i<3;i++) if (obstacle==g_R11Mover[i]) obstacleZone=1;
        }
        if (g_R11FReady)
            for (new i=0;i<R11F_COUNT;i++) if (obstacle==g_R11FMover[i]) obstacleZone=2;
    }
    if (obstacleZone<0) return FMRES_IGNORED;
    new Float:p[3],zone=Room11_PlayerZone(id,p);
    if (g_R11Ready && zone==1 && obstacleZone==1) g_R11BlockedUid[id]=get_user_userid(id);
    else if (g_R11FReady && zone==2)
    {
        if (obstacleZone==2 || (obstacleZone==0 && Room11_ContactsGateAt(id,true,p)))
            g_R11FBlockedUid[id]=get_user_userid(id);
    }
    return FMRES_IGNORED;
}

public Room11Contact_PreThink(id)
{
    if (!g_PluginReady) return FMRES_IGNORED;
    new Float:p[3], zone=Room11_PlayerZone(id,p);
    if (g_R11Ready)
    {
        if (zone==1)
        {
            Room11_PreThinkInZone(id,p);
            // A return may have moved the player: match the old second hook's fresh read.
            if (g_R11FReady) zone=Room11_PlayerZone(id,p);
        }
        else g_R11BlockedUid[id]=0;
    }
    if (g_R11FReady)
    {
        if (zone==2) Room11Flight_PreThinkInZone(id,p);
        else g_R11FBlockedUid[id]=0;
    }
    return FMRES_IGNORED;
}
