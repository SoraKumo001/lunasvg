import { optimizeImage } from 'wasm-image-optimization';
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
                const svgBuffer = fs.readFileSync(inputPath);
                
                // wasm-image-optimization: specify format as 'png'
                const pngBuffer = await optimizeImage({
                    image: svgBuffer,
                    format: 'png',
                    quality: 100
                });

                if (pngBuffer) {
                    fs.writeFileSync(outputPath, pngBuffer);
                    console.log(`Converted (WASM): ${file} -> ${path.basename(outputPath)}`);
                } else {
                    console.error(`Failed to convert ${file}: Received empty buffer`);
                }
            } catch (err) {
                console.error(`Failed to convert ${file}:`, err);
            }
        }
    }
    console.log('WASM batch conversion complete.');
}

const args = process.argv.slice(2);
if (args.length < 2) {
    console.log('Usage: pnpm tsx tools/convert_wasm.ts <input_dir> <output_dir>');
    process.exit(1);
}

convert(args[0], args[1]);
