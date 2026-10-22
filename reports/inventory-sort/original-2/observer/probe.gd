extends Node
func _ready():call_deferred("run")
func run():
    var fixture=globaldata.fixture
    for locale in fixture.translations:
        var translation=Translation.new()
        translation.locale=locale
        for key in fixture.translations[locale]:translation.add_message(key,fixture.translations[locale][key])
        TranslationServer.add_translation(translation)
    var pairs=[]
    var rows=[]
    for locale in fixture.locales:
        TranslationServer.set_locale(locale)
        print("SORT_LOCALE "+JSON.print({"requested":locale,"actual":TranslationServer.get_locale()}))
        assert(TranslationServer.get_locale()==locale)
        var comparator=Inventory.new(Inventory.InvType.STORAGE)
        for key in fixture.translations[locale]:assert(comparator.tr(key)==fixture.translations[locale][key])
        for a in fixture.names:
            for b in fixture.names:
                var first=Item.new(a,false,int(fixture.items[a].get("doses",1)),1000)
                var second=Item.new(b,false,int(fixture.items[b].get("doses",1)),1001)
                pairs.append({"locale":locale,"a":a,"b":b,"less":comparator._sort_auto(first,second)})
        for row in fixture.cases:
            var inventory=Inventory.new(Inventory.InvType.STORAGE)
            for index in range(row.names.size()):
                var name=row.names[index]
                var doses=int(row.spray_doses) if name==fixture.spray else int(fixture.items[name].get("doses",1))
                assert(inventory.add_item(Item.new(name,false,doses,1000+index)))
            var before=inventory.to_array()
            inventory.sort_auto()
            var once=inventory.to_array()
            inventory.sort_auto()
            var twice=inventory.to_array()
            rows.append({"locale":locale,"index":row.index,"before":before,"once":once,"twice":twice})
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"pairs":pairs,"rows":rows},"  "))
    file.close()
    print("INVENTORY_SORT_REFERENCE_COMPLETE "+str(rows.size()))
    get_tree().quit()
