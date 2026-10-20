extends SceneTree
func _init():call_deferred("run")
func run():
    var config=File.new()
    assert(config.open("res://input.json",File.READ)==OK)
    var paths=JSON.parse(config.get_as_text()).result
    config.close()
    var result={"engine":Engine.get_version_info(),"textures":[]}
    for row in paths:
        var texture=load("res://"+row.path)
        assert(texture is StreamTexture)
        var image=texture.get_data()
        image.convert(Image.FORMAT_RGBA8)
        var file=File.new()
        assert(file.open("res://pixels-"+str(row.id)+".bin",File.WRITE)==OK)
        file.store_buffer(image.get_data())
        file.close()
        result.textures.append({"id":row.id,"path":row.path,"width":image.get_width(),"height":image.get_height(),"flags":texture.flags,"format":image.get_format()})
    var file=File.new()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print(result,"  "))
    file.close()
    print("DUNGEON_TEXTURE_REFERENCE_COMPLETE")
    quit()
