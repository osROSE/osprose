/*
    Rose Online Server Emulator
    Copyright (C) 2006,2007 OSRose Team http://www.osrose.net

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

    depeloped with Main erose/hrose source server + some change from the original eich source
*/

#include "worldmap.h"

CMap::CMap( )
{
    id = 0;
    dayperiod = 0;
    morningtime = 0;
    daytime = 0;
    eveningtime = 0;
    nighttime = 0;
    allowpvp = 0;
    allowpat = 0;
    MapTime = 0;
    LastUpdate = clock( );
    CurrentTime = 0;
    RespawnList.clear();
    MonsterSpawnList.clear();
#ifdef USEIFO
    MobGroupList.clear();
#endif
    MonsterList.clear();
    DropsList.clear();
    PlayerList.clear();
    NPCList.clear();
    TeleGateList.clear();
    MonsterMutex = PTHREAD_MUTEX_INITIALIZER;
    DropMutex = PTHREAD_MUTEX_INITIALIZER;
}

CMap::~CMap( )
{
    for(UINT i=0;i<RespawnList.size();i++)
        delete RespawnList.at(i);
#ifdef USEIFO
    for(UINT i = 0; i < MobGroupList.size(); i++) {
      CMobGroup *thisgroup = MobGroupList.at(i);
      for (UINT j = 0; j < thisgroup->basicMobs.size(); j++)
        delete thisgroup->basicMobs.at(j);
      for (UINT j = 0; j < thisgroup->tacMobs.size(); j++)
        delete thisgroup->tacMobs.at(j);
      delete thisgroup;
    }
#endif
    for(UINT i=0;i<MonsterSpawnList.size();i++)
        delete MonsterSpawnList.at(i);
    for(UINT i=0;i<MonsterList.size();i++)
        delete MonsterList.at(i);
    for(UINT i=0;i<DropsList.size();i++)
        delete DropsList.at(i);
    for(UINT i=0;i<NPCList.size();i++)
        delete NPCList.at(i);
    for(UINT i=0;i<TeleGateList.size();i++)
        delete TeleGateList.at(i);
}

bool CMap::AddPlayer( CPlayer* player )
{
    PlayerList.push_back( player );
    return false;
}

bool CMap::RemovePlayer( CPlayer* player, bool clearobject )
{
    GServer->ClearClientID( player->clientid );
    if(clearobject)
    {
        BEGINPACKET( pak, 0x794 );
        ADDWORD    ( pak, player->clientid );
        GServer->SendToVisible( &pak, player, false );
    }
    for(UINT i=0;i<PlayerList.size();i++)
    {
        if(PlayerList.at(i)==player)
        {
            PlayerList.erase( PlayerList.begin()+i );
            return true;
        }
    }
    return false;
}


// add a new monster to this map
CMonster* CMap::AddMonster( UINT montype, fPoint position, UINT owner, UINT spawnid , UINT aggro, bool IsTD, UINT limit, UINT AI)
{
    // check if is a valid monster
    CNPCData* thisnpc = GServer->GetNPCDataByID( montype );
    if(thisnpc == NULL)
    {
        Log( MSG_WARNING, "Invalid montype %i", montype );
        return NULL;
    }
    CMonster* monster = new (nothrow) CMonster( position, montype, this->id, owner, spawnid  );
    if(monster == NULL)
    {
        Log( MSG_WARNING, "Error allocing memory" );
        return NULL;
    }
    monster->thisnpc = thisnpc;
    monster->SetStats( );
    monster->Stats->MaxHP = monster->GetMaxHP( );
    monster->Stats->HP = monster->Stats->MaxHP;
    monster->Stats->MaxMP = monster->GetMaxMP( );
    monster->Stats->MP = monster->Stats->MaxMP;
    monster->thisnpc->_EntityType = ENTITY_MONSTER;
    monster->Status->spawnid = spawnid; //this way we can easily find which spawn a mob belongs to
    if(AI != 0)
    {
        //Set monster's AI to the defined value
        monster->monAI = AI;
        CAip* script = NULL;
        for(unsigned j=0; j < GServer->AipList.size(); j++)
        {
            if (GServer->AipList.at(j)->AInumber == AI)
            {
                script = GServer->AipList.at(j);
                break;
            }
        }
        if(script == NULL)
        {
            Log( MSG_WARNING, "Invalid AI script for monster type %i using AI %i", montype,AI );
            monster->AItimer = 4000;
        }
        else
        {
            monster->AItimer = script->minTime * 1000; //set AI timer value for this monster
        }
        if (monster->AItimer == 0)
            monster->AItimer = 4000;
    }
    else
    {
        //Set monster's AI to the default value for this monster type
        monster->monAI = monster->thisnpc->AI; 
        CAip* script = NULL;
        for(unsigned j=0; j < GServer->AipList.size(); j++)
        {
            if (GServer->AipList.at(j)->AInumber == monster->monAI)
            {
                script = GServer->AipList.at(j);
                break;
            }
        }
        if(script == NULL)
        {
            Log( MSG_WARNING, "Invalid AI script for monster type %i using AI %i", montype,AI );
            monster->AItimer = 4000;
        }
        else
        {
            monster->AItimer = script->minTime * 1000; //set AI timer value for this monster
        }
        if (monster->AItimer == 0)
            monster->AItimer = 4000;
    }   
    if(limit == 0)
    {
        monster->Status->dayonly = false;
        monster->Status->nightonly = false;
    }
    if(limit == 1)
    {
        monster->Status->dayonly = true;
        monster->Status->nightonly = false;
    }
    if(limit == 2)
    {
        monster->Status->dayonly = false;
        monster->Status->nightonly = true;
    }
    if(aggro != 1)
        monster->thisnpc->aggresive = aggro;
    monster->Stats->HP = monster->Stats->MaxHP;
        /*                    BEGINPACKET( pak, 0x799 );
                            ADDWORD    ( pak, monster->clientid );
                            ADDWORD    ( pak, monster->clientid );
                            ADDWORD    ( pak, 0x8005 );
                            GServer->SendToVisible( &pak, monster);*/
    MonsterList.push_back( monster );
    if(spawnid != 0)
    {
#ifndef USEIFO
        CSpawnArea* thisspawn = GServer->GetSpawnArea( spawnid, this->id );
        if(thisspawn!=NULL)
            thisspawn->amon++;
#else
        CMobGroup* thisgroup = GServer->GetMobGroup( spawnid, this->id );
        if(thisgroup!=NULL)
            thisgroup->active++;
#endif
    }
    monster->activecycle = GServer->RandNumber(1, 100);
    monster->SpawnTime = clock( );
    monster->lastAiUpdate = clock();
    monster->hitcount = 0xFF;
    //monster->OnSpawn( false );
    monster->hitcount = 0;
    return monster;
}


// Delete a monster
bool CMap::DeleteMonster( CMonster* monster, bool clearobject, UINT i )
{
    if(monster == NULL) return false;
    GServer->ClearClientID( monster->clientid );
    if(monster->Position->respawn != 0)
    {
#ifndef USEIFO
        CSpawnArea* thisspawn = GServer->GetSpawnArea( monster->Position->respawn, monster->Position->Map );
        if(thisspawn!=NULL)
        {
            if(thisspawn->amon >= thisspawn->max)// reset spawn timer if the spawn is full
                thisspawn->lastRespawnTime = clock();
            thisspawn->amon--;
        }
#else
        CMobGroup* thisgroup = GServer->GetMobGroup( monster->Position->respawn, monster->Position->Map );
        if(thisgroup!=NULL)
        {
            if(thisgroup->active >= thisgroup->limit)// reset spawn timer if the spawn is full
                thisgroup->lastRespawnTime = clock();
            thisgroup->active--;
            thisgroup->basicKills++;
        }
#endif
    }
    if(clearobject)
    {
        BEGINPACKET( pak, 0x799 );
        ADDWORD    ( pak, monster->clientid );
        ADDWORD    ( pak, monster->clientid );
        ADDWORD    ( pak, 0x8005 );
        //ADDDWORD   ( pak, monster->Stats->HP );
        //ADDDWORD   ( pak, 16 );
        GServer->SendToVisible( &pak, monster );
    }
    if(i != 0)
    {
        MonsterList.erase( MonsterList.begin()+i );
        delete monster;
        return true;
    }
    for(UINT i=0;i<MonsterList.size();i++)
    {
        CMonster* othermon = MonsterList.at(i);
        if(othermon == monster)
        {
            MonsterList.erase( MonsterList.begin() + i );
            delete monster;
            return true;
        }
    }
    delete monster;
    return false;
}

//add a new drop
bool CMap::AddDrop( CDrop* drop )
{
    drop->droptime = time(NULL);
    DropsList.push_back( drop );
    return true;
}

bool CMap::DeleteDrop( CDrop* drop )
{
    BEGINPACKET( pak, 0x794 );
	ADDWORD    ( pak, drop->clientid );
	GServer->SendToVisible( &pak, drop );
    GServer->ClearClientID( drop->clientid );
    for(UINT j=0;j<DropsList.size();j++)
    {
        if(drop==DropsList.at(j))
        {
            DropsList.erase( DropsList.begin()+j );
            delete drop;
            return true;
        }
    }
    delete drop;
    return false;
}

bool CMap::AddNPC( CNPC* npc )
{
    NPCList.push_back( npc );
    return true;
}

bool CMap::DeleteNPC( CNPC* npc )
{
    BEGINPACKET( pak, 0x794 );
    ADDWORD    ( pak, npc->clientid );
    GServer->SendToMap( &pak, npc->posMap );
    GServer->ClearClientID( npc->clientid );
    Log(MSG_INFO,"Sent message to players in map to delete NPC");
    for(UINT j=0;j<NPCList.size();j++)
    {
        if(npc == NPCList.at(j))
        {
            NPCList.erase( NPCList.begin()+j );
            delete npc;
            return true;
        }
    }
    delete npc;
    return false;
}
