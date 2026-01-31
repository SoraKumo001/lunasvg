import { Resvg } from '@resvg/resvg-js';
import fs from 'fs';
import path from 'path';

async function convert(inputDir: string, outputDir: string) {
    if (!fs.existsSync(inputDir)) {
        console.error(`Error: Input directory does not exist: ${inputDir}`);
        process.exit(1);
    }

    if (!fs.existsSync(outputDir)) {
        fs.mkdirSync(outputDir, { recursive: true });
    }

    const files = fs.readdirSync(inputDir);
    for (const file of files) {
        if (file.endsWith('.svg')) {
            const inputPath = path.join(inputDir, file);
            const outputPath = path.join(outputDir, file.replace('.svg', '.png'));

            try {
                const svg = fs.readFileSync(inputPath);
                const resvg = new Resvg(svg);
                const pngData = resvg.render();
                const pngBuffer = pngData.asPng();

                fs.writeFileSync(outputPath, pngBuffer);
                console.log(`Converted: ${file} -> ${path.basename(outputPath)}`);
            } catch (err) {
                console.error(`Failed to convert ${file}:`, err);
            }
        }
    }
    console.log('Batch conversion complete.');
}

const args = process.argv.slice(2);
if (args.length < 2) {
    console.log('Usage: pnpm tsx tools/convert.ts <input_dir> <output_dir>');
    process.exit(1);
}

convert(args[0], args[1]);
