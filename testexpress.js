import http from 'k6/http';

export default function () {
  http.get('http://httpexpress:3000/');
}