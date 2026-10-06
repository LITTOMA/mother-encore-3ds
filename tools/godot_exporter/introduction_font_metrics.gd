# Introduction's complete DynamicFont metric contract. Keep independent from
# gameplay glyph selection and its numeric contextual-pair probe.
extends SceneTree

func _init():
 var version=Engine.get_version_info()
 if version.major!=3 or version.minor!=6 or version.patch!=2 or version.hash!="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8":
  quit(7)
  return
 var input=File.new()
 if input.open("res://request.json",File.READ)!=OK:
  quit(2)
  return
 var request=JSON.parse(input.get_as_text()).result
 var faces=[]
 for spec in request:
  var font=load("res://"+spec.source)
  var advances=[]
  for cp in spec.codepoints:
   advances.append(font.get_char_size(int(cp)).x)
  var data=[font.font_data]
  for i in range(font.get_fallback_count()):
   data.append(font.get_fallback(i))
  var settings=[]
  for d in data:
   settings.append({"path":d.font_path.trim_prefix("res://"),"antialiased":d.antialiased,"hinting":d.hinting})
  var pairs=[]
  var widths=[]
  for text in spec.texts:
   widths.append(font.get_string_size(text).x)
   for i in range(text.length()):
    var cp=text.ord_at(i)
    var following=text.ord_at(i+1) if i+1<text.length() else 0
    pairs.append({"codepoint":cp,"next":following,"advance":font.get_char_size(cp,following).x})
  faces.append({"source":spec.source,"size":font.size,"ascent":font.get_ascent(),"descent":font.get_descent(),"height":font.get_height(),"advances":advances,"pairs":pairs,"texts":spec.texts,"widths":widths,"char_spacing":font.extra_spacing_char,"space_spacing":font.extra_spacing_space,"data_settings":settings})
 var output=File.new()
 if output.open("res://metrics.json",File.WRITE)!=OK:
  quit(3)
  return
 output.store_string(JSON.print({"version":version,"faces":faces}))
 output.close()
 quit()
