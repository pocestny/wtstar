import resolve from 'rollup-plugin-node-resolve';
import scss from 'rollup-plugin-scss'
import alias from 'rollup-plugin-alias'
import { string } from "rollup-plugin-string"

module.exports = {
  input: './home.js',
  output: {
    file: '../../_build/web/home.js',
    format:'iife'
  },
  plugins:[
    resolve(),
    scss({output:true}),
    string({include:'**/*.txt'})
  ]
};
