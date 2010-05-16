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
#include "worldserver.h"

// Map Process
PVOID MapProcess( PVOID TS )
{
    bool only_npc = false;
    while(GServer->ServerOnline)
    {
        pthread_mutex_lock( &GServer->PlayerMutex );
        pthread_mutex_lock( &GServer->MapMutex );
        for(UINT i=0;i<GServer->MapList.Map.size();i++)
        {
            CMap* map = GServer->MapList.Map.at(i);
            if( map->PlayerList.size()<1 )
                only_npc = true;
            else
                only_npc = false;
                //continue;
            if(!only_npc)
            {
                // Player update //------------------------
                for(UINT j=0;j<map->PlayerList.size();j++)
                {
                    CPlayer* player = map->PlayerList.at(j);
                    if(!player->Session->inGame) continue;
                    if(player->IsDead( )) continue;
                    //if(player->UpdateValues( ))
                        player->UpdatePosition( );
                    if(player->IsOnBattle( ))
                        player->DoAttack( );
                    player->RefreshBuff( );
                    player->PlayerHeal( );
                    player->Regeneration( );
                    player->CheckPlayerLevelUP( );
                    if( GServer->Config.AUTOSAVE == 1 )
                    {
                        clock_t etime = clock() - player->lastSaveTime;
                        if( etime >= GServer->Config.SAVETIME*1000 )
                        {
                            player->savedata( );
                            player->lastSaveTime = clock();
                        }
                    }
                }
                // Monster update //------------------------
                pthread_mutex_lock( &map->MonsterMutex );
                for(UINT j=0;j<map->MonsterList.size();j++)
                {
                    CMonster* monster = map->MonsterList.at(j);
                    
                    UINT thistimer = monster->AItimer; 
    				if(monster->hitcount == 0xFF)//this is a delay for new monster spawns this might olso fix invisible monsters(if they attack directly on spawning the client dosn't get the attack packet(its not in it's visible list yet))
                    {
                        if(thistimer < (UINT)GServer->round((clock( ) - monster->lastAiUpdate)))
                        {
                            monster->hitcount = 0;
                            //monster->DoAi(monster->thisnpc->AI, 0);
                            monster->DoAi(monster->monAI, 0);
                            monster->lastAiUpdate=clock();
                        }
                    }
                    //still need this for special spawns
                    if(!map->IsNight( ) && monster->Status->nightonly)// if day, delete all night time monsters
                    {
                        //Log( MSG_INFO, "Night Only monster deleted. Type %i", monster->montype);
                        map->DeleteMonster( monster, true, j );
                        continue;
                    }
                    if(map->IsNight() && monster->Status->dayonly)
                    {
                        //Log( MSG_INFO, "Day Only monster deleted. Type %i", monster->montype);
                        map->DeleteMonster( monster, true, j );
                        continue;
                    }
    
                    if(!monster->PlayerInRange( )) continue;
                    if(!monster->UpdateValues( )) continue;
                        monster->UpdatePosition( );
                    if(monster->IsOnBattle( ))
                    {
                        //monster->DoAttack( );
                        if(thistimer<(UINT)GServer->round((clock( ) - monster->lastAiUpdate)))
                        {
                             //monster->DoAi(monster->thisnpc->AI, 2);
                             monster->DoAi(monster->monAI, 2);
                             monster->lastAiUpdate = clock();
                             //Log(MSG_INFO,"Monster type: %i current HP: %i",monster->montype, monster->Stats->HP);
                        }
                        else
                        {
                             //Log(MSG_INFO,"Monster doing attack");
                             monster->DoAttack( );
                        }
                    }
                    else if(!monster->IsOnBattle() && !monster->IsDead( ))
                    {
                        if(thistimer<(UINT)GServer->round((clock( ) - monster->lastAiUpdate)))
                        {
                            //monster->DoAi(monster->thisnpc->AI, 1);
                            monster->DoAi(monster->monAI, 1);
                            monster->lastAiUpdate = clock();
                        }
                    }
                    monster->RefreshBuff( );
                    if (monster->IsSummon())
                    {
                        monster->SummonUpdate(monster,map, j);
                        continue;
                    }
                    if(monster->IsDead( ))
                    {
                        //monster->DoAi(monster->thisnpc->AI, 5);
                        monster->DoAi(monster->monAI, 5);
                        monster->OnDie( );
                    }
                }
            }
            if(only_npc)
                pthread_mutex_lock( &map->MonsterMutex );
            
            //AIP for NPCs
            for(UINT j=0;j<map->NPCList.size();j++)
            {
                CNPC* npc = map->NPCList.at(j);
                if(npc->thisnpc->AI != 0)
                {
                     CAip* script = NULL;
                     for(unsigned j=0; j < GServer->AipList.size(); j++)
                     {
                         if (GServer->AipList.at(j)->AInumber == npc->thisnpc->AI)
                         {
                             script = GServer->AipList.at(j);
                             break;
                         }
                     }
                     if(script == NULL)
                     {
                         //Log( MSG_WARNING, "Invalid AI script for AI %i", npc->thisnpc->AI );
                         continue;
                     }
                     UINT thistimer = script->minTime * 1000; //seems to be set in seconds in AIP
                     if(thistimer<(UINT)GServer->round((clock( ) - npc->lastAiUpdate))) //check AIP conditions when the timer calls for it
                     {
                         CNPCData* thisnpc = GServer->GetNPCDataByID( npc->npctype );
                         if(thisnpc == NULL)
                         {
                             Log( MSG_WARNING, "Invalid montype %i", npc->npctype );
                             continue;
                         }
                         CMonster* monster = new (nothrow) CMonster( npc->pos, npc->npctype, map->id, 0, 0  );
                         monster->thisnpc = thisnpc;
                         monster->CharType = 4;
                         int AIP = monster->thisnpc->AI;
                         int lma_previous_eventID = npc->thisnpc->eventid;
                         monster->DoAi(monster->thisnpc->AI, 1);
                         //check if eventID changed, if we do it in AIP conditions / actions, it just fails...
                         if (lma_previous_eventID != npc->thisnpc->eventid)
                         {
                             //Log(MSG_WARNING,"Event ID not the same NPC %i from %i to %i in map %i, npc->thisnpc->eventid=%i !", npc->npctype, lma_previous_eventID, monster->thisnpc->eventid, map->id, npc->thisnpc->eventid);
                             npc->thisnpc->eventid = monster->thisnpc->eventid;
                             BEGINPACKET( pak, 0x790 );
                             ADDWORD    ( pak, npc->clientid );
                             ADDWORD    ( pak, npc->thisnpc->eventid );
                             GServer->SendToMap(&pak, map->id);
                         }


                         if(AIP == GServer->Config.AIWatch)
                             Log(MSG_DEBUG,"NPC AI number %i successfully run",monster->thisnpc->AI);
                         if(monster->IsOnBattle())
                             monster->DoAttack( );
                         map->DeleteMonster(monster);
                         //delete monster;
                         npc->lastAiUpdate = clock();
                         if(AIP == GServer->Config.AIWatch)
                             Log(MSG_DEBUG,"LastAiUpdate clock reset");
                     }
                }

                //LMA: Sometimes another NPC does the job for you.
                if(npc->thisnpc->eventid != GServer->ObjVar[npc->npctype][0])
                {
                    int new_event_id = GServer->ObjVar[npc->npctype][0];
                    //Log(MSG_WARNING,"(2)Event ID not the same NPC %i from %i to %i in map %i, npc->thisnpc->eventid=%i !",npc->npctype,npc->thisnpc->eventid,new_event_id,map->id,npc->thisnpc->eventid);
                    npc->thisnpc->eventid = new_event_id;
                    //LMA: We have to change the event ID here since we didn't send the clientID :(
                    BEGINPACKET( pak, 0x790 );
                    ADDWORD    ( pak, npc->clientid );
                    ADDWORD    ( pak, npc->thisnpc->eventid );
                    GServer->SendToMap(&pak,map->id);
                }
            }
            pthread_mutex_unlock( &map->MonsterMutex );
        }
        pthread_mutex_unlock( &GServer->MapMutex );
        pthread_mutex_unlock( &GServer->PlayerMutex );
        #ifdef _WIN32
        Sleep(GServer->Config.MapDelay);
        #else
        usleep(GServer->Config.MapDelay);
        #endif
    }
    pthread_exit( NULL );
}

// Visibility Process
PVOID VisibilityProcess(PVOID TS)
{
    while(GServer->ServerOnline)
    {
        pthread_mutex_lock( &GServer->PlayerMutex );
        pthread_mutex_lock( &GServer->MapMutex );
        for(UINT i=0;i<GServer->MapList.Map.size();i++)
        {
            CMap* map = GServer->MapList.Map.at(i);
            map->CleanDrops( );//moved for test
            map->RespawnMonster( );//moved for test
            if( map->PlayerList.size()<1 )
                continue;
            for(UINT j=0;j<map->PlayerList.size();j++)
            {
                CPlayer* player = map->PlayerList.at(j);
                if(!player->Session->inGame) continue;
                if(!player->VisiblityList()) Log(MSG_WARNING, "Visibility False: %u", player->clientid );
                
            }
        }
        pthread_mutex_unlock( &GServer->MapMutex );
        pthread_mutex_unlock( &GServer->PlayerMutex );
        #ifdef _WIN32
        Sleep(GServer->Config.VisualDelay);
        #else
        usleep(GServer->Config.VisualDelay);
        #endif
    }
    pthread_exit(NULL);
}

// World Process
PVOID WorldProcess( PVOID TS )
{
    while( GServer->ServerOnline )
    {
        pthread_mutex_lock( &GServer->MapMutex );
        for(UINT i=0;i<GServer->MapList.Map.size();i++)
        {
            CMap* map = GServer->MapList.Map.at(i);
            if( map->PlayerList.size()<1 )
                continue;
            map->UpdateTime( );
            pthread_mutex_lock( &map->DropMutex );
            //map->CleanDrops( );
            pthread_mutex_unlock( &map->DropMutex );
            pthread_mutex_lock( &map->MonsterMutex );
            //map->RespawnMonster( );
            pthread_mutex_unlock( &map->MonsterMutex );
        }
        pthread_mutex_unlock( &GServer->MapMutex );
        GServer->RefreshFairy( );
        #ifdef _WIN32
        Sleep(GServer->Config.WorldDelay);
        #else
        usleep(GServer->Config.WorldDelay);
        #endif
    }
    pthread_exit(NULL);
}

// Shutdown Server Process
void ShutdownServer(PVOID TS)
{
    int minutes = (int)TS;
    #ifdef _WIN32
    Sleep(minutes*60000);
    #else
    usleep(minutes*60000);
    #endif
    Log( MSG_INFO, "Saving User Information... " );
    GServer->DisconnectAll();
    Log( MSG_INFO, "Waiting Process to ShutDown... " );
    GServer->ServerOnline = false;
    int status,res;
    res = pthread_join( GServer->WorldThread[0], (PVOID*)&status );
    if(res)
    {
        Log( MSG_WARNING, "World thread can't be joined" );
    }
    else
    {
        Log( MSG_INFO, "World thread closed." );
    }
    res = pthread_join( GServer->WorldThread[1], (PVOID*)&status );
    if(res)
    {
        Log( MSG_WARNING, "Visibility thread can't be joined" );
    }
    else
    {
        Log( MSG_INFO, "Visibility thread closed." );
    }
    res = pthread_join( GServer->MapThread[0], (PVOID*)&status );
    if(res)
    {
        Log( MSG_WARNING, "Map thread can't be joined" );
    }
    else
    {
        Log( MSG_INFO, "Map thread closed." );
    }
    Log( MSG_INFO, "All Threads Closed." );
    GServer->isActive = false;
    pthread_exit(NULL);
}
