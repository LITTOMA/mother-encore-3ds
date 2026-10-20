extends Node
var currentCamera=self
var runner
func shake_camera(magnitude,duration,direction,interval,weight,diminish):
    runner.event({"kind":"shake","magnitude":magnitude,"duration":duration,"direction":[direction.x,direction.y],"interval":interval,"weight":weight,"diminish":diminish})
