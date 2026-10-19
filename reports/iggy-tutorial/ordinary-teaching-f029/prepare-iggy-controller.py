from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'build/full-port/run_player7d_baseline.py').read_text()
s=s.replace('Actual recorded save -> ordinary LOAD/B/Goods/door/Goods, two cold runs.','Actual recorded save -> ordinary LOAD/walk/Iggy choices/teaching/learned Bat, two cold runs.')
s=s.replace('7d2e552f86c9ef0c995c85c301e3a2f26a78f32a','f0296baf53d03285975c0fe9aa1639943a831911').replace('player7d-baseline-v1','iggy-f029-teaching-v1')
s=s.replace("a=p.parse_args()","p.add_argument('--walk-plan',type=Path,required=True);a=p.parse_args()")
s=s.replace("projections=[]","route=json.loads(a.walk_plan.read_bytes());assert route['waypoints'][0]==[8,-39]and route['waypoints'][-1]==[1300,4713];shutil.copyfile(a.walk_plan,out/'walk-plan.json')\nprojections=[]")
old=(r/'build/full-port/run_cemetery_record.py').read_text();helpers=old[old.index(' def travel('):old.index(" wait(lambda s:s['title_phase']==1)")];helpers='\n'.join(' '+line if line else''for line in helpers.splitlines())+'\n'
helpers=helpers.replace('     if predicate(s):return s',"     if predicate(s)or(s['world_programme']==6 and s['world_stage']!=0):return s")
helpers=helpers.replace("\n   now=state();delta=value-now[axis]","\n   now=state()\n   if now['world_programme']==6 and now['world_stage']!=0:return\n   delta=value-now[axis]")
helpers=helpers.replace("\n    now=state();delta=value-now[axis]","\n    now=state()\n    if now['world_programme']==6 and now['world_stage']!=0:return\n    delta=value-now[axis]")
helpers=helpers.replace("   now=state();assert abs(now['player_x']-x)","   now=state()\n   if now['world_programme']==6 and now['world_stage']!=0:return\n   assert abs(now['player_x']-x)")
s=s.replace("  wait(lambda s:s['title_phase']==1);",helpers+"  wait(lambda s:s['title_phase']==1);")
s=s.replace("projections.append(projection(after_field));",'')
s=s.replace(";flags={", ";programme=room['sections']['Program'][6];programme_commands=room['sections']['Command'][programme['first_command']:programme['first_command']+programme['command_count']];last_wait=[c for c in programme_commands if c['opcode']==3 and c['phrase']==15];last_poll=[c for c in programme_commands if c['opcode']==26 and c['phrase']==15];assert len(last_wait)==1 and last_wait[0]['duration']==3 and len(last_poll)==1 and last_poll[0]['duration']==0;flags={")
insert='''  for i,(x,y)in enumerate(route['waypoints']):
   precise_point(x,y)
   now=state()
   if now['world_programme']==6 and now['world_stage']!=0:break
   assert now['world_stage']==0 and now['field_scene']==2 and now['field_phase']==0,('Unexpected source event during planned approach',i,now)
   if i%18==0:record('walk_'+str(i))
  record('iggy_approach');capture('iggy_approach')
  assert state()['world_programme']==6 and state()['world_stage']!=0,('Source destination Area was not entered',state())
  record('teaching_started');capture('teaching_started')
  choices=0;visits=[];last=None;flag_at=None;finish_frame=None
  for tick in range(2400):
   now=state();ident=now['world_dialogue']
   if ident!=0xffffffff and ident!=last:
    assert 3100<=ident<=3113,('Unexpected teaching text',now)
    last=ident;visits.append(ident);record('teaching_text_'+str(len(visits))+'_'+str(ident))
   if flag_at is None and flag(now,'bat'):flag_at=now['frame'];record('bat_flag_set')
   if now['world_stage']==0 and flag(now,'bat')and now['world_dialogue']==0xffffffff:
    finish_frame=now['frame'];break
   if now['choice_phase']==2:
    record('choice_'+str(choices));capture('choice_'+str(choices))
    if repetition==2 and choices==0:pulse('RIGHT');pulse('A')
    elif repetition==2 and choices==1:pulse('B')
    else:pulse('A')
    choices+=1
   elif now['dialogue_active']and(now['dialogue_finished']or now['dialogue_stopped']):pulse('A')
   else:advance(8)
  else:raise TimeoutError('Teaching did not finish '+str(state()))
  assert choices==(1 if repetition==1 else 3),(choices,visits)
  # Probe frame counts publications, not 60Hz physics ticks. main.cpp advances
  # idle timers from osGetTime elapsed milliseconds; require observable order,
  # checked actual three-second resource and the independent core180-tick test.
  assert flag_at is not None and finish_frame>flag_at,(flag_at,finish_frame)
  advance(15);completed=record('teaching_completed');capture('teaching_completed')
  assert completed['key_count']==loaded['key_count']+1 and completed['inventory_count']==loaded['inventory_count']
  pos=(completed['player_x'],completed['player_y']);assert abs(pos[0]-1432)<.01 and abs(pos[1]-4719)<.01
  client.call('input.set',{'buttons':['B']});advance(4);client.call('input.clear');capture('learned_bat')
  client.call('input.set',{'buttons':['RIGHT']});during=advance(4);client.call('input.clear')
  assert(during['player_x'],during['player_y'])==pos,('Learned Bat did not hold movement during ATTACK',during)
  advance(80);neutral=record('bat_finished');assert neutral['title_phase']==0 and neutral['width']==400 and neutral['height']==240 and neutral['key_count']==completed['key_count']
  client.call('input.set',{'buttons':['RIGHT']});moved=advance(8);client.call('input.clear');assert moved['player_x']>pos[0],('MOVE did not resume after Bat',moved)
  advance(10);frontier=record('frontier');capture('frontier');projections.append(projection(frontier))
  records['teaching_summary']=dict(choices=choices,text_visits=visits,flag_frame=flag_at,finish_frame=finish_frame,bat_movement_held=True,move_resumed=True)
'''
s=s.replace("  result=dict(status='passed'",insert+"  result=dict(status='passed'")
s=s.replace("  def projection(s):return", "  def flag(s,name):\n   i=flags[name];return bool(s['story_flag_'+str(i//32)]&(1<<(i%32)))\n  def projection(s):return")
s=s.replace('Two cold baseline runs passed with identical result projection; owned profiles removed','Two ordinary teaching runs passed with identical gameplay result projection; owned profiles removed')
compile(s,'run_iggy_f029.py','exec');(r/'build/full-port/run_iggy_f029.py').write_text(s,encoding='utf8');print('Prepared ordinary-input controller; second cold boot exercises no and cancel before yes')
