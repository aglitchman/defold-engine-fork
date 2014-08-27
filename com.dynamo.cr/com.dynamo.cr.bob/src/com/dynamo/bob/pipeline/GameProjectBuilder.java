package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.fs.IResource;

/**
 * Game project and disk archive builder.
 * @author chmu
 *
 */
@BuilderParams(name = "GameProjectBuilder", inExts = ".project", outExt = "", createOrder = 1000)
public class GameProjectBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        TaskBuilder<Void> builder = Task.<Void> newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(".projectc"));
        if (project.option("build_disk_archive", "false").equals("true")) {
            builder.addOutput(input.changeExt(".darc"));
        }

        project.buildResource(input, CopyCustomResourcesBuilder.class);

        for (Task<?> task : project.getTasks()) {
            for (IResource output : task.getOutputs()) {
                builder.addInput(output);
            }
        }

        return builder.build();
    }

    private File createArchive(Task<Void> task) throws IOException {
        RandomAccessFile outFile = null;
        File tempArchiveFile = File.createTempFile("tmp", "darc");
        tempArchiveFile.deleteOnExit();
        outFile = new RandomAccessFile(tempArchiveFile, "rw");
        outFile.setLength(0);

        String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());
        ArchiveBuilder ab = new ArchiveBuilder(root);
        boolean doCompress = project.option("compress_disk_archive_entries", "false").equals("true");
        int i = 0;
        for (IResource input : task.getInputs()) {
            if (i > 0) {
                // First input is game.project
                //
                // 2:d argument is true to use compression.
                // We then try to compress all entries.
                // If the compressed/uncompressed ratio > 0.95 we do not compress
                // to save on load time...
                ab.add(input.getAbsPath(), doCompress);
            }
            ++i;
        }

        ab.write(outFile);
        outFile.close();
        return tempArchiveFile;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FileInputStream is = null;
        try {
            if (project.option("build_disk_archive", "false").equals("true")) {
                File archiveFile = createArchive(task);
                is = new FileInputStream(archiveFile);
                IResource arcOut = task.getOutputs().get(1);
                arcOut.setContent(is);
                archiveFile.delete();
            }

            IResource in = task.getInputs().get(0);
            IResource projOut = task.getOutputs().get(0);
            projOut.setContent(in.getContent());

        } finally {
            IOUtils.closeQuietly(is);
        }
    }
}

