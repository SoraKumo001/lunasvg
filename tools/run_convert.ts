import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';

// OSごとの実行ファイル名を決定
const isWindows = process.platform === 'win32';
const binName = isWindows ? 'convert.exe' : 'convert';
const binPath = path.resolve(__dirname, '../dist', binName);

// コマンドライン引数を取得 (images/svg images_output など)
const args = process.argv.slice(2);

if (!fs.existsSync(binPath)) {
    console.error(`Error: Binary not found at ${binPath}`);
    console.error(`Please build the project first using cmake.`);
    process.exit(1);
}

// バイナリを実行
const child = spawn(binPath, args, { stdio: 'inherit' });

child.on('exit', (code) => {
    process.exit(code ?? 0);
});
