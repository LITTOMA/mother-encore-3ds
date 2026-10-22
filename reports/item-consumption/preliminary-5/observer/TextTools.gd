extends Reference
class_name TextTools
static func format_text_with_context(key,target,_context,value):
    return JSON.print([key,target.get_name(),value])
