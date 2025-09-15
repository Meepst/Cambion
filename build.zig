const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const os = target.result.os.tag;
    const optimize = b.standardOptimizeOption(.{});

    const glfw_dep = b.dependency("glfw", .{});
    const glfw_lib = glfw_dep.artifact("glfw");

    glfw_lib.linkLibC();
    b.installArtifact(glfw_lib);

    const glm_path = b.path("dependencies");
    const glm_lib = b.addStaticLibrary(.{
        .name = "glm",
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
    });
    glm_lib.addCSourceFiles(.{
        .root = glm_path,
        .flags = &.{},
        .files = &.{
            "glm/detail/glm.cpp",
        },
    });
    glm_lib.addIncludePath(glm_path);
    glm_lib.installHeadersDirectory(glm_path, "", .{
        .include_extensions = &.{
            ".h",
            ".hpp",
            ".inl",
        },
    });

    glm_lib.linkLibCpp();
    b.installArtifact(glm_lib);

    const exe_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
    });
    switch (os) {
        .windows => {
            exe_mod.addLibraryPath(.{ .cwd_relative = "C:/Windows/System32" });
            exe_mod.linkSystemLibrary("vulkan-1", .{ .needed = true });
        },
        .linux => {
            exe_mod.linkSystemLibrary("vulkan", .{ .needed = true });
        },
        else => unreachable,
    }

    exe_mod.linkLibrary(glfw_lib);
    exe_mod.linkLibrary(glm_lib);

    const exe = b.addExecutable(.{
        .name = "Cambion",
        .root_module = exe_mod,
    });
    exe.addCSourceFiles(.{
        .root = b.path("src"),
        .flags = &.{},
        .files = &.{
            "main.cpp",
        },
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
