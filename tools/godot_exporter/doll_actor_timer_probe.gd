extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(delta):runner.physics_tick(delta)
    func _process(delta):runner.idle_tick(delta)
var driver
var frame=0
var timer
var rows=[]
var output=""
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output=arg.trim_prefix("--encore-out=")
    assert(output != "")
    driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
func physics_tick(delta):
    if frame==0:
        timer=create_timer(1)
        timer.connect("timeout",self,"expired")
    rows.append([frame,"physics", "%.17f"%delta,"%.17f"%timer.time_left])
func idle_tick(delta):
    if timer:rows.append([frame,"idle","%.17f"%delta,"%.17f"%timer.time_left])
    create_timer(0).connect("timeout",self,"capture")
func capture():
    if timer:rows.append([frame,"capture","%.17f"%timer.time_left])
    frame+=1
    if frame==64:
        var f=File.new()
        assert(f.open(output,File.WRITE)==OK)
        f.store_string(JSON.print(rows,"  "))
        f.close()
        quit()
func expired():rows.append([frame,"expired","%.17f"%timer.time_left])
