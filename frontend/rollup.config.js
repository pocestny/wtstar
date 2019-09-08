import scss from 'rollup-plugin-scss'
import { string } from "rollup-plugin-string";

module.exports = {
  input: './main.js',
  output: {
    file: './frontend.js',
    format: 'iife'
  },
  plugins:[
    scss({output:'./frontend.css'}),
    string({include:'**/*.txt'})
  ]
};
