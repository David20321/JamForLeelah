bl_info = {
    "name": "Export Wolfire Jam For Leelah format (.txt)",
    "author": "David Rosen",
    "version": (1, 0, 0),
    "blender": (2, 73, 0),
    "location": "File > Export > Wolfire JFL (.txt)",
    "description": "The script exports Blender geometry to Wolfire's JFL format.",
    "category": "Import-Export",
}

import bpy
from bpy.props import StringProperty, EnumProperty, BoolProperty
import mathutils

def ExportWJFL(path):
    rig_obj = bpy.context.selected_objects[0]
    if rig_obj is None:
        print("No objects selected")
        return  
    #Find Mesh child
    if rig_obj.type == 'MESH':
        mesh_obj = rig_obj
    else:
        mesh_obj = None
        for child in rig_obj.children:
            if child.type == 'MESH':
                mesh_obj = child
    if mesh_obj is None:
        print("Could not find mesh")
        return
    mesh = mesh_obj.data
    #Find Armature data
    armature = None
    if rig_obj.type == 'ARMATURE':
        armature = rig_obj.data
    #Start writing file
    file =  open(path,"w")
    file.write("Wolfire JamForLeelah Format\n")
    file.write("Version 1\n")    
    file.write("--BEGIN--\n")    
    #Export mesh
    file.write("Mesh\n")
    for vert in mesh.vertices:
        file.write("  Vert %d\n" % vert.index)
        file.write("    Coords: (%f, %f, %f)\n" % (vert.co[0], vert.co[1], vert.co[2]))
        file.write("    Normals: (%f, %f, %f)\n" % (vert.normal[0], vert.normal[1], vert.normal[2]))
        file.write("    Vertex Groups:\n" )
        if armature != None:
            for group in vert.groups:
                if group.weight > 0.01:
                    file.write("      \"%s\", %f\n"%(mesh_obj.vertex_groups[group.group].name, group.weight))
    uv_layer = mesh.uv_layers.active.data
    for poly in mesh.polygons:
        file.write("  Polygon index: %d, length: %d\n" % (poly.index, poly.loop_total))
        for loop_index in range(poly.loop_start, poly.loop_start + poly.loop_total):
            file.write("    Vertex: %d\n" % mesh.loops[loop_index].vertex_index)
            file.write("    UV: (%f, %f)\n" % (uv_layer[loop_index].uv[0], uv_layer[loop_index].uv[1]))
    if armature != None:
        #Save editor state to restore later
        start_pose_position = armature.pose_position
        start_action = rig_obj.animation_data.action
        start_frame = bpy.context.scene.frame_current
        #Export bones
        armature.pose_position = 'REST'
        bpy.context.scene.frame_set(start_frame) #Updates matrices?
        file.write("Skeleton\n")
        for bone in rig_obj.pose.bones:
            if bone.name[:4] == "DEF-":
                file.write("  Bone: "+bone.name+"\n")
                matrix_final = rig_obj.matrix_world * bone.matrix
                file.write("    Matrix: (")
                for i in range(0,16):
                    file.write("%f"%matrix_final[int(i/4)][i%4])
                    if i != 15:
                        file.write(", ")
                    else:
                        file.write(")\n")
                parent = bone.parent
                while parent != None and parent.name[:4] != "DEF-":
                    name_check = "DEF-"+parent.name[4:]
                    if parent.name[:4] == "ORG-" and rig_obj.pose.bones[name_check] != None:
                        parent = rig_obj.pose.bones[name_check]
                    else:
                        parent = parent.parent
                if parent != None:
                    file.write("    Parent: \"%s\"\n"%parent.name)
                else:
                    file.write("    Parent: \"\"\n")       
        #Export animation
        armature.pose_position = 'POSE'
        for action in bpy.data.actions:
            file.write("Action: "+action.name+"\n")
            print(action.name)
            print(action.frame_range)
            start = int(round(action.frame_range[0]))
            end   = int(round(action.frame_range[1])) 
            rig_obj.animation_data.action = action
            for frame in range(start, end):
                bpy.context.scene.frame_set(frame)
                file.write("  Frame: "+str(frame)+"\n")
                for bone in rig_obj.pose.bones:
                    if bone.name[:4] == "DEF-":
                        file.write("    Bone: "+bone.name+"\n")
                        #TODO: this matrix printing code is just copy-pasted from above
                        matrix_final = rig_obj.matrix_world * bone.matrix
                        file.write("      Matrix: (")
                        for i in range(0,16):
                            file.write("%f"%matrix_final[int(i/4)][i%4])
                            if i != 15:
                                file.write(", ")
                            else:
                                file.write(")\n")
        armature.pose_position = start_pose_position
        rig_obj.animation_data.action = start_action
        bpy.context.scene.frame_set(start_frame)   
    file.write("--END--\n") 
    file.close()

class WJFLExporter(bpy.types.Operator):
    bl_idname = "export.wjfl"
    bl_label = "Export Wolfire JFL"
    filepath = StringProperty(subtype='FILE_PATH')

    def execute(self, context):
        filePath = bpy.path.ensure_ext(self.filepath, ".txt")
        ExportWJFL(filePath)
        return {'FINISHED'}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = bpy.path.ensure_ext(bpy.data.filepath, ".txt")
        WindowManager = context.window_manager
        WindowManager.fileselect_add(self)
        return {'RUNNING_MODAL'}

def menu_func(self, context):
    self.layout.operator(WJFLExporter.bl_idname, text="Wolfire JFL (.txt)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()