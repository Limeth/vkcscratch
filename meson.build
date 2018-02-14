project('vkcscratch', 'c')

vulkan = dependency('vulkan')

# Just output in both ways
run_command('glslangValidator', 'shader/shader.comp', '-V', '-l', '-o', 'src/shader_data.h', '--vn', 'shader')
run_command('glslangValidator', 'shader/shader.comp', '-V', '-l', '-o', 'shader/shader.spv')

executable('vkcscratch', 'src/main.c', dependencies: vulkan)
