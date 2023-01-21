import http from 'k6/http';

export default function () {
  http.get('http://http11:8080/');
}